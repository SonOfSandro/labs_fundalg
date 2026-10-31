#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

typedef enum {
    STATUS_OK,
    STATUS_ERR_INVALID_ARG_FORMAT,
    STATUS_ERR_NULL_PTR,
    STATUS_ERR_RANGE,
    STATUS_ERR_MEM,
    STATUS_ERR_IO
} StatusCode;

static void print_error(const StatusCode status) {
    if (status == STATUS_ERR_INVALID_ARG_FORMAT) {
        printf("Error: Invalid arguments passed to function.\n");
    } else if (status == STATUS_ERR_NULL_PTR) {
        printf("Error: Null pointer encountered.\n");
    } else if (status == STATUS_ERR_RANGE) {
        printf("Error: Value exceeds numerical limits or precision lost.\n");
    } else if (status == STATUS_ERR_MEM) {
        printf("Error: Memory allocation failed.\n");
    } else if (status == STATUS_ERR_IO) {
        printf("Error: Input/Output operation failed.\n");
    } else {
        printf("Error: Unknown execution failure.\n");
    }
}

static StatusCode shift_polynomial(const double eps, const double a, double **const result,
                                   const int degree, ...) {
    if (result == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *result = NULL;

    if (!isfinite(eps) || eps <= 0.0 || !isfinite(a) || degree < 0) {
        return STATUS_ERR_INVALID_ARG_FORMAT;
    }

    const size_t count = (size_t)degree + 1;

    if (count > SIZE_MAX / sizeof(long double) || count > SIZE_MAX / sizeof(double)) {
        return STATUS_ERR_RANGE;
    }

    long double *f = (long double *)malloc(count * sizeof(long double));
    long double *g = (long double *)calloc(count, sizeof(long double));
    double *answer = (double *)malloc(count * sizeof(double));

    if (f == NULL || g == NULL || answer == NULL) {
        free(f);
        free(g);
        free(answer);
        return STATUS_ERR_MEM;
    }

    va_list args;
    va_start(args, degree);
    StatusCode st = STATUS_OK;

    for (size_t i = 0; i < count; ++i) {
        f[i] = va_arg(args, double);
        if (!isfinite((double)f[i])) {
            st = STATUS_ERR_INVALID_ARG_FORMAT;
            break;
        }
    }
    va_end(args);

    if (st != STATUS_OK) {
        free(f);
        free(g);
        free(answer);
        return st;
    }

    for (size_t i = count; i > 0; --i) {
        const size_t used = count - i;
        for (size_t j = used; j > 0; --j) {
            g[j] = g[j - 1] + (long double)a * g[j];
            if (!isfinite((double)g[j])) {
                st = STATUS_ERR_RANGE;
                break;
            }
        }

        if (st != STATUS_OK) {
            break;
        }

        g[0] = (long double)a * g[0] + f[i - 1];
        if (!isfinite((double)g[0])) {
            st = STATUS_ERR_RANGE;
            break;
        }
    }

    if (st == STATUS_OK) {
        for (size_t i = 0; i < count; ++i) {
            if (fabsl(g[i]) > DBL_MAX) {
                st = STATUS_ERR_RANGE;
                break;
            }

            answer[i] = (double)g[i];

            if (fabsl(g[i] - (long double)answer[i]) > eps) {
                st = STATUS_ERR_RANGE;
                break;
            }
        }
    }

    free(f);
    free(g);

    if (st != STATUS_OK) {
        free(answer);
        return st;
    }

    *result = answer;
    return STATUS_OK;
}

int main(void) {
    double *g = NULL;
    StatusCode st = shift_polynomial(1e-9, 2.0, &g, 2, 1.0, 2.0, 3.0);

    if (st == STATUS_OK) {
        for (int i = 0; i <= 2; ++i) {
            if (printf("g%d = %.17g\n", i, g[i]) < 0) {
                st = STATUS_ERR_IO;
                break;
            }
        }
    }

    free(g);

    if (st != STATUS_OK) {
        print_error(st);
        return 1;
    }

    if (fflush(stdout) == EOF) {
        print_error(STATUS_ERR_IO);
        return 1;
    }

    return 0;
}