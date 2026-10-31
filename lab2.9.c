#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>

typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO } Status;

static long long gcd(long long a, long long b) {
    while (b) {
        long long temp = a % b;
        a = b;
        b = temp;
    }
    return a;
}

static bool has_finite_representation(double value, int base, double epsilon) {
    if (value <= epsilon || value >= 1.0 - epsilon || base < 2) return false;
    long long numerator = 0, denominator = 1;
    for (long long current_denominator = 1; current_denominator <= 1000000; current_denominator++) {
        long long current_numerator = (long long)round(value * current_denominator);
        if (fabs((double)current_numerator / current_denominator - value) <= epsilon) {
            numerator = current_numerator;
            denominator = current_denominator;
            break;
        }
    }
    if (numerator == 0) return false;
    long long common_divisor = gcd(numerator, denominator);
    denominator /= common_divisor;
    for (long long divisor = 2; divisor * divisor <= denominator; divisor++) {
        while (denominator % divisor == 0) {
            if (base % divisor != 0) return false;
            denominator /= divisor;
        }
    }
    return denominator == 1 || base % denominator == 0;
}

static Status check_finite_representations(int base, double epsilon, int count, bool **const result, ...) {
    if (!result || base < 2 || count <= 0 || !isfinite(epsilon) || epsilon <= 0) return ERR_ARG;
    *result = NULL;
    if ((size_t)count > SIZE_MAX / sizeof(bool)) return ERR_RANGE;
    bool *answers = malloc((size_t)count * sizeof(*answers));
    if (!answers) return ERR_MEM;
    va_list arguments;
    va_start(arguments, result);
    Status status = OK;
    for (int i = 0; i < count; ++i) {
        double value = va_arg(arguments, double);
        if (!isfinite(value) || value <= 0.0 || value >= 1.0) {
            status = ERR_ARG;
            break;
        }
        answers[i] = has_finite_representation(value, base, epsilon);
    }
    va_end(arguments);
    if (status != OK) {
        free(answers);
        return status;
    }
    *result = answers;
    return OK;
}

int main(void) {
    Status status = OK;
    bool *results = NULL;
    status = check_finite_representations(10, 1e-9, 5, &results, 0.5, 0.25, 0.1, 0.2, 0.3);
    if (status == OK) {
        if (printf("Base 10:\n") < 0) status = ERR_IO;
        const double values[] = {0.5, 0.25, 0.1, 0.2, 0.3};
        for (int i = 0; i < 5 && status == OK; ++i)
            if (printf("  %.10g: %s\n", values[i], results[i] ? "finite" : "infinite") < 0) status = ERR_IO;
        free(results);
        results = NULL;
    }
    if (status == OK) {
        status = check_finite_representations(2, 1e-9, 4, &results, 0.5, 0.25, 0.1, 0.75);
        if (status == OK) {
            if (printf("Base 2:\n") < 0) status = ERR_IO;
            const double values[] = {0.5, 0.25, 0.1, 0.75};
            for (int i = 0; i < 4 && status == OK; ++i)
                if (printf("  %.10g: %s\n", values[i], results[i] ? "finite" : "infinite") < 0) status = ERR_IO;
            free(results);
            results = NULL;
        }
    }
    if (status == OK) {
        status = check_finite_representations(8, 1e-9, 3, &results, 0.125, 0.5, 0.1);
        if (status == OK) {
            if (printf("Base 8:\n") < 0) status = ERR_IO;
            const double values[] = {0.125, 0.5, 0.1};
            for (int i = 0; i < 3 && status == OK; ++i)
                if (printf("  %.10g: %s\n", values[i], results[i] ? "finite" : "infinite") < 0) status = ERR_IO;
            free(results);
            results = NULL;
        }
    }
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    return status != OK || fflush(stdout) == EOF;
}