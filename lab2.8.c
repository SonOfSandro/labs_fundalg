#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO } Status;

static int digit_value(const int character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'A' && character <= 'Z') return character - 'A' + 10;
    if (character >= 'a' && character <= 'Z' + 6) return character - 'a' + 10;
    return -1;
}

static Status validate_number(const char *const text, const int base, size_t *const length) {
    if (!text || !*text || !length || base < 2 || base > 36) return ERR_ARG;
    size_t count = 0;
    for (; text[count]; ++count) {
        const int value = digit_value((unsigned char) text[count]);
        if (value < 0 || value >= base) return ERR_ARG;
    }
    *length = count;
    return OK;
}

static Status add_numbers(const int base, const char *const first, const char *const second,
                          char **const result) {
    if (!result) return ERR_ARG;
    *result = NULL;
    size_t first_length = 0, second_length = 0;
    Status status = validate_number(first, base, &first_length);
    if (status == OK) status = validate_number(second, base, &second_length);
    if (status != OK) return status;
    const size_t maximum = first_length > second_length ? first_length : second_length;
    if (maximum > SIZE_MAX - 2) return ERR_RANGE;
    char *sum = malloc(maximum + 2);
    if (!sum) return ERR_MEM;
    size_t position = maximum + 1;
    sum[position] = '\0';
    int carry = 0;
    while (first_length || second_length || carry) {
        int digit = carry;
        if (first_length) digit += digit_value((unsigned char) first[--first_length]);
        if (second_length) digit += digit_value((unsigned char) second[--second_length]);
        carry = digit / base;
        digit %= base;
        sum[--position] = (char) (digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
    while (sum[position] == '0' && sum[position + 1]) ++position;
    const size_t length = maximum + 1 - position;
    char *text = malloc(length + 1);
    if (!text) {
        free(sum);
        return ERR_MEM;
    }
    memcpy(text, sum + position, length + 1);
    free(sum);
    *result = text;
    return OK;
}

static Status sum_numbers(const int base, const int count, char **const result, ...) {
    if (!result || base < 2 || base > 36 || count <= 0) return ERR_ARG;
    *result = NULL;
    char *total = NULL;
    Status status = OK;
    va_list arguments;
    va_start(arguments, result);
    for (int i = 0; i < count && status == OK; ++i) {
        const char *const number = va_arg(arguments, const char *);
        size_t length = 0;
        status = validate_number(number, base, &length);
        if (status != OK) break;
        if (!total) status = add_numbers(base, number, "0", &total);
        else {
            char *next = NULL;
            status = add_numbers(base, total, number, &next);
            if (status == OK) {
                free(total);
                total = next;
            }
        }
    }
    va_end(arguments);
    if (status != OK) {
        free(total);
        return status;
    }
    *result = total;
    return OK;
}

int main(void) {
    Status status = OK;
    char *sum = NULL;
    status = sum_numbers(16, 2, &sum, (const char *) "FFFF", (const char *) "1");
    if (status == OK && puts(sum) == EOF) status = ERR_IO;
    free(sum);
    sum = NULL;
    if (status == OK) {
        status = sum_numbers(2, 4, &sum, (const char *) "1011", (const char *) "1",
                             (const char *) "0", (const char *) "000101");
        if (status == OK && puts(sum) == EOF) status = ERR_IO;
        free(sum);
        sum = NULL;
    }
    if (status == OK) {
        status = sum_numbers(36, 3, &sum, (const char *) "ZZ", (const char *) "1", (const char *) "zz");
        if (status == OK && puts(sum) == EOF) status = ERR_IO;
        free(sum);
        sum = NULL;
    }
    if (status == OK) {
        status = sum_numbers(10, 3, &sum, (const char *) "0000", (const char *) "00", (const char *) "0");
        if (status == OK && puts(sum) == EOF) status = ERR_IO;
        free(sum);
        sum = NULL;
    }
    if (status == OK) {
        const Status expected = sum_numbers(10, 2, &sum, (const char *) "12X", (const char *) "1");
        free(sum);
        sum = NULL;
        if (expected != ERR_ARG) status = ERR_RANGE;
    }
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    return status != OK || fflush(stdout) == EOF;
}
