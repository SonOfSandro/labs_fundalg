#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

typedef enum {
    STATUS_OK,
    STATUS_ERR_INVALID_ARGS_COUNT,
    STATUS_ERR_UNKNOWN_FLAG,
    STATUS_ERR_NULL_PTR,
    STATUS_ERR_INVALID_SEED,
    STATUS_ERR_RANGE,
    STATUS_ERR_MEM,
    STATUS_ERR_IO
} StatusCode;

static size_t get_rand_index(const size_t limit) {
    if (limit == 0) {
        return 0;
    }
    size_t r = 0;
    for (size_t i = 0; i < sizeof(size_t); ++i) {
        r = (r << 15) ^ ((size_t)rand() & 0x7FFFU);
    }
    return r % (limit + 1);
}

static StatusCode mode_l(const char *const text, size_t *const length) {
    if (text == NULL || length == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    size_t n = 0;
    while (text[n] != '\0') {
        if (n == SIZE_MAX) {
            return STATUS_ERR_RANGE;
        }
        ++n;
    }
    *length = n;
    return STATUS_OK;
}

static StatusCode mode_r(const char *const text, char **const result) {
    if (text == NULL || result == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *result = NULL;

    size_t n = 0;
    StatusCode st = mode_l(text, &n);
    if (st != STATUS_OK) {
        return st;
    }
    if (n == SIZE_MAX) {
        return STATUS_ERR_RANGE;
    }

    char *copy = (char *)malloc((n + 1) * sizeof(char));
    if (copy == NULL) {
        return STATUS_ERR_MEM;
    }

    for (size_t i = 0; i < n; ++i) {
        copy[i] = text[n - 1 - i];
    }
    copy[n] = '\0';

    *result = copy;
    return STATUS_OK;
}

static StatusCode mode_u(const char *const text, char **const result) {
    if (text == NULL || result == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *result = NULL;

    size_t n = 0;
    StatusCode st = mode_l(text, &n);
    if (st != STATUS_OK) {
        return st;
    }
    if (n == SIZE_MAX) {
        return STATUS_ERR_RANGE;
    }

    char *copy = (char *)malloc((n + 1) * sizeof(char));
    if (copy == NULL) {
        return STATUS_ERR_MEM;
    }

    for (size_t i = 0; i < n; ++i) {
        char c = text[i];
        if ((i % 2 != 0) && (c >= 'a' && c <= 'z')) {
            c = (char)(c - 'a' + 'A');
        }
        copy[i] = c;
    }
    copy[n] = '\0';

    *result = copy;
    return STATUS_OK;
}

static StatusCode mode_n(const char *const text, char **const result) {
    if (text == NULL || result == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *result = NULL;

    size_t n = 0;
    StatusCode st = mode_l(text, &n);
    if (st != STATUS_OK) {
        return st;
    }
    if (n == SIZE_MAX) {
        return STATUS_ERR_RANGE;
    }

    char *copy = (char *)malloc((n + 1) * sizeof(char));
    if (copy == NULL) {
        return STATUS_ERR_MEM;
    }

    size_t pos = 0;
    for (int group = 0; group < 3; ++group) {
        for (size_t i = 0; i < n; ++i) {
            const char c = text[i];
            int kind = 2;
            if (c >= '0' && c <= '9') {
                kind = 0;
            } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                kind = 1;
            }

            if (kind == group) {
                copy[pos++] = c;
            }
        }
    }
    copy[pos] = '\0';

    *result = copy;
    return STATUS_OK;
}

static StatusCode mode_c(const char *const *const words, const size_t count,
                         const unsigned int seed, char **const result) {
    if (words == NULL || count == 0 || result == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *result = NULL;

    if (count > SIZE_MAX / sizeof(size_t)) {
        return STATUS_ERR_RANGE;
    }

    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t n = 0;
        StatusCode st = mode_l(words[i], &n);
        if (st != STATUS_OK) {
            return st;
        }
        if (n > SIZE_MAX - 1 - total) {
            return STATUS_ERR_RANGE;
        }
        total += n;
    }

    size_t *order = (size_t *)malloc(count * sizeof(size_t));
    char *copy = (char *)malloc((total + 1) * sizeof(char));
    if (order == NULL || copy == NULL) {
        free(order);
        free(copy);
        return STATUS_ERR_MEM;
    }

    for (size_t i = 0; i < count; ++i) {
        order[i] = i;
    }

    srand(seed);
    for (size_t i = count - 1; i > 0; --i) {
        const size_t j = get_rand_index(i);
        const size_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    size_t pos = 0;
    for (size_t i = 0; i < count; ++i) {
        const char *const current_word = words[order[i]];
        for (size_t j = 0; current_word[j] != '\0'; ++j) {
            copy[pos++] = current_word[j];
        }
    }
    copy[pos] = '\0';

    free(order);
    *result = copy;
    return STATUS_OK;
}

static StatusCode parse_seed(const char *const str, unsigned int *const result) {
    if (str == NULL || result == NULL || *str == '\0') {
        return STATUS_ERR_INVALID_SEED;
    }

    const char *ptr = str;
    while (*ptr == ' ' || *ptr == '\t') {
        ++ptr;
    }

    if (*ptr == '+') {
        ++ptr;
    } else if (*ptr == '-') {
        return STATUS_ERR_INVALID_SEED;
    }

    if (*ptr < '0' || *ptr > '9') {
        return STATUS_ERR_INVALID_SEED;
    }

    unsigned int value = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        const unsigned int digit = (unsigned int)(*ptr - '0');
        if (value > (UINT_MAX - digit) / 10U) {
            return STATUS_ERR_RANGE;
        }
        value = value * 10U + digit;
        ++ptr;
    }

    while (*ptr == ' ' || *ptr == '\t') {
        ++ptr;
    }

    if (*ptr != '\0') {
        return STATUS_ERR_INVALID_SEED;
    }

    *result = value;
    return STATUS_OK;
}

static void print_error(const StatusCode status) {
    if (status == STATUS_ERR_INVALID_ARGS_COUNT) {
        fprintf(stderr, "Error: Invalid number of command line arguments.\n");
    } else if (status == STATUS_ERR_UNKNOWN_FLAG) {
        fprintf(stderr, "Error: Unknown or malformed flag. Expected -l, -r, -u, -n, or -c.\n");
    } else if (status == STATUS_ERR_NULL_PTR) {
        fprintf(stderr, "Error: Null pointer encountered.\n");
    } else if (status == STATUS_ERR_INVALID_SEED) {
        fprintf(stderr, "Error: Invalid seed format (must be an unsigned integer).\n");
    } else if (status == STATUS_ERR_RANGE) {
        fprintf(stderr, "Error: Value or string length exceeds numerical limits (overflow).\n");
    } else if (status == STATUS_ERR_MEM) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
    } else if (status == STATUS_ERR_IO) {
        fprintf(stderr, "Error: Input/Output operation failed.\n");
    } else {
        fprintf(stderr, "Error: Unknown execution error.\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        print_error(STATUS_ERR_INVALID_ARGS_COUNT);
        return 1;
    }

    const char *const flag = argv[1];

    if (flag[0] != '-' || flag[1] == '\0' || flag[2] != '\0') {
        print_error(STATUS_ERR_UNKNOWN_FLAG);
        return 1;
    }

    const char mode = flag[1];
    StatusCode st = STATUS_OK;
    char *result = NULL;
    size_t length = 0;

    if (mode == 'l') {
        if (argc != 3) {
            print_error(STATUS_ERR_INVALID_ARGS_COUNT);
            return 1;
        }
        st = mode_l(argv[2], &length);
    } else if (mode == 'r') {
        if (argc != 3) {
            print_error(STATUS_ERR_INVALID_ARGS_COUNT);
            return 1;
        }
        st = mode_r(argv[2], &result);
    } else if (mode == 'u') {
        if (argc != 3) {
            print_error(STATUS_ERR_INVALID_ARGS_COUNT);
            return 1;
        }
        st = mode_u(argv[2], &result);
    } else if (mode == 'n') {
        if (argc != 3) {
            print_error(STATUS_ERR_INVALID_ARGS_COUNT);
            return 1;
        }
        st = mode_n(argv[2], &result);
    } else if (mode == 'c') {
        if (argc < 4) {
            print_error(STATUS_ERR_INVALID_ARGS_COUNT);
            return 1;
        }

        unsigned int seed = 0;
        st = parse_seed(argv[3], &seed);
        if (st == STATUS_OK) {
            const size_t count = (size_t)argc - 3;
            if (count > SIZE_MAX / sizeof(char *)) {
                st = STATUS_ERR_RANGE;
            } else {
                const char **words = (const char **)malloc(count * sizeof(*words));
                if (words == NULL) {
                    st = STATUS_ERR_MEM;
                } else {
                    words[0] = argv[2];
                    for (size_t i = 1; i < count; ++i) {
                        words[i] = argv[i + 3];
                    }
                    st = mode_c(words, count, seed, &result);
                    free(words);
                }
            }
        }
    } else {
        print_error(STATUS_ERR_UNKNOWN_FLAG);
        return 1;
    }

    if (st == STATUS_OK) {
        if (mode == 'l') {
            if (printf("%zu\n", length) < 0) {
                st = STATUS_ERR_IO;
            }
        } else {
            if (puts(result) == EOF) {
                st = STATUS_ERR_IO;
            }
        }
    }

    free(result);

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