#if defined( MINGW32 ) && !defined(__USE_MINGW_ANSI_STDIO)
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO } Status;

typedef struct { const double *coordinates; size_t dimension; } Vector;
typedef Status (*Norm)(const Vector *, const void *, double, double *);
typedef struct { const double *values; size_t dimension; } Matrix;
typedef struct { Vector *vectors; size_t count; double maximum; } Longest;

static Status validate_vector(const Vector *const vector, const double eps, double *const result) {
    if (!vector || !vector->coordinates || !vector->dimension || !result || !isfinite(eps) || eps <= 0) return ERR_ARG;
    for (size_t i = 0; i < vector->dimension; ++i) if (!isfinite(vector->coordinates[i])) return ERR_ARG;
    return OK;
}

static Status norm_inf(const Vector *const vector, const void *const context, const double eps, double *const result) {
    (void)context;
    Status status = validate_vector(vector, eps, result);
    if (status != OK) return status;
    double maximum = 0;
    for (size_t i = 0; i < vector->dimension; ++i) maximum = fmax(maximum, fabs(vector->coordinates[i]));
    *result = maximum; return OK;
}

static Status norm_p(const Vector *const vector, const void *const context, const double eps, double *const result) {
    Status status = validate_vector(vector, eps, result);
    if (status != OK || !context) return ERR_ARG;
    const double power = *(const double *)context;
    if (!isfinite(power) || power < 1.0) return ERR_ARG;
    double scale;
    status = norm_inf(vector, NULL, eps, &scale);
    if (status != OK) return status;
    if (scale == 0.0) { *result = 0.0; return OK; }
    long double sum = 0;
    for (size_t i = 0; i < vector->dimension; ++i)
        sum += powl(fabsl((long double)vector->coordinates[i] / scale), power);
    const long double answer = scale * powl(sum, 1.0L / power);
    if (!isfinite(answer) || answer > DBL_MAX) return ERR_RANGE;
    *result = (double)answer; return OK;
}

static Status norm_matrix(const Vector *const vector, const void *const context, const double eps, double *const result) {
    Status status = validate_vector(vector, eps, result);
    if (status != OK || !context) return ERR_ARG;
    const Matrix *matrix = context;
    const size_t dimension = vector->dimension;
    if (!matrix->values || matrix->dimension != dimension) return ERR_ARG;
    if (dimension > SIZE_MAX / dimension / sizeof(long double)) return ERR_RANGE;
    for (size_t i = 0; i < dimension; ++i) for (size_t j = 0; j < dimension; ++j) {
        if (!isfinite(matrix->values[i*dimension+j]) || fabsl((long double)matrix->values[i*dimension+j] - matrix->values[j*dimension+i]) > eps) return ERR_ARG;
    }
    long double *cholesky = calloc(dimension*dimension, sizeof(*cholesky));
    if (!cholesky) return ERR_MEM;
    for (size_t i = 0; i < dimension && status == OK; ++i) for (size_t j = 0; j <= i; ++j) {
        long double value = ((long double)matrix->values[i*dimension+j] + matrix->values[j*dimension+i]) / 2;
        for (size_t k = 0; k < j; ++k) value -= cholesky[i*dimension+k] * cholesky[j*dimension+k];
        if (!isfinite(value)) { status = ERR_RANGE; break; }
        if (i == j) {
            if (value <= eps) { status = ERR_ARG; break; }
            cholesky[i*dimension+j] = sqrtl(value);
        } else cholesky[i*dimension+j] = value / cholesky[j*dimension+j];
    }
    long double length = 0;
    if (status == OK) for (size_t j = 0; j < dimension; ++j) {
        long double coordinate = 0;
        for (size_t i = j; i < dimension; ++i) coordinate += cholesky[i*dimension+j] * vector->coordinates[i];
        length = hypotl(length, coordinate);
    }
    free(cholesky);
    if (status != OK) return status;
    if (!isfinite(length) || length > DBL_MAX) return ERR_RANGE;
    *result = (double)length; return OK;
}

static void free_longest(Longest *const results, const size_t count) {
    for (size_t i = 0; i < count; ++i) free(results[i].vectors);
    free(results);
}

static Status longest_vectors(const size_t dimension, const double eps, const int vector_count,
                              const int norm_count, Longest **const result, ...) {
    if (!result) return ERR_ARG;
    *result = NULL;
    if (!dimension || vector_count <= 0 || norm_count <= 0 || !isfinite(eps) || eps <= 0) return ERR_ARG;
    if ((size_t)vector_count > SIZE_MAX / sizeof(Vector) || (size_t)vector_count > SIZE_MAX / sizeof(double) ||
        (size_t)norm_count > SIZE_MAX / sizeof(Longest)) return ERR_RANGE;
    Vector *vectors = malloc((size_t)vector_count * sizeof(*vectors));
    double *values = malloc((size_t)vector_count * sizeof(*values));
    Longest *found = calloc((size_t)norm_count, sizeof(*found));
    if (!vectors || !values || !found) { free(vectors); free(values); free(found); return ERR_MEM; }
    Status status = OK;
    va_list arguments;
    va_start(arguments, result);
    for (int i = 0; i < vector_count; ++i) {
        vectors[i] = va_arg(arguments, Vector);
        if (vectors[i].dimension != dimension || validate_vector(&vectors[i], eps, &values[i]) != OK) status = ERR_ARG;
    }
    for (int k = 0; k < norm_count && status == OK; ++k) {
        Norm norm = va_arg(arguments, Norm);
        const void *context = va_arg(arguments, const void *);
        if (!norm) { status = ERR_ARG; break; }
        double maximum = 0;
        for (int i = 0; i < vector_count && status == OK; ++i) {
            status = norm(&vectors[i], context, eps, &values[i]);
            if (status == OK && (!isfinite(values[i]) || values[i] < -eps)) status = ERR_RANGE;
            if (status == OK) maximum = fmax(maximum, values[i]);
        }
        if (status != OK) break;
        found[k].vectors = malloc((size_t)vector_count * sizeof(Vector));
        if (!found[k].vectors) { status = ERR_MEM; break; }
        found[k].maximum = maximum;
        for (int i = 0; i < vector_count; ++i)
            if (fabs(values[i] - maximum) <= eps) found[k].vectors[found[k].count++] = vectors[i];
    }
    va_end(arguments);
    free(vectors); free(values);
    if (status != OK) { free_longest(found, (size_t)norm_count); return status; }
    *result = found; return OK;
}

int main(void) {
    const double first[] = {3, 4}, second[] = {5, 0}, third[] = {-3, -4};
    const Vector vector_a = {first, 2}, vector_b = {second, 2}, vector_c = {third, 2};
    const double power = 2, matrix_values[] = {2, 0, 0, 1};
    const Matrix matrix = {matrix_values, 2};
    Longest *found = NULL;
    Status status = longest_vectors(2, 1e-9, 3, 3, &found, vector_a, vector_b, vector_c,
                                    (Norm)norm_inf, (const void *)NULL, (Norm)norm_p, (const void *)&power,
                                    (Norm)norm_matrix, (const void *)&matrix);
    if (status == OK) {
        for (int k = 0; k < 3; ++k) {
            if (printf("Norm %d, maximum %.12g, vectors %zu\n", k + 1, found[k].maximum, found[k].count) < 0) status = ERR_IO;
            for (size_t i = 0; i < found[k].count; ++i)
                if (printf("  %.12g %.12g\n", found[k].vectors[i].coordinates[0], found[k].vectors[i].coordinates[1]) < 0) status = ERR_IO;
        }
        free_longest(found, 3);
    }
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    return status != OK || fflush(stdout) == EOF;
}