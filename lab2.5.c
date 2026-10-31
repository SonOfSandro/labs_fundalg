#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>

typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO } Status;

static Status read_line(FILE *const input, char **const result, bool *const exists) {
    if (!input || !result || !exists) return ERR_ARG;
    *result = NULL; *exists = false;
    size_t capacity = 128, used = 0;
    char *line = malloc(capacity);
    if (!line) return ERR_MEM;
    int character; Status status = OK;
    while ((character = fgetc(input)) != EOF) {
        *exists = true;
        if (character == '\n') break;
        if (character == 0) { status = ERR_ARG; break; }
        if (used == capacity - 1) {
            if (capacity > SIZE_MAX / 2) { status = ERR_RANGE; break; }
            char *next = realloc(line, capacity * 2);
            if (!next) { status = ERR_MEM; break; }
            line = next; capacity *= 2;
        }
        line[used++] = (char)character;
    }
    if (ferror(input)) status = ERR_IO;
    if (status != OK) { free(line); return status; }
    line[used] = '\0'; *result = line;
    return OK;
}

static Status fields(char *line, char **const words, const size_t count) {
    if (!line || !words) return ERR_ARG;
    size_t found = 0;
    while (*line) {
        while (isspace((unsigned char)*line)) ++line;
        if (!*line) break;
        if (found == count) return ERR_ARG;
        words[found++] = line;
        while (*line && !isspace((unsigned char)*line)) ++line;
        if (*line) *line++ = '\0';
    }
    return found == count ? OK : ERR_ARG;
}

static Status parse_unsigned(const char *string, unsigned long long *const result) {
    if (!string || !*string || !result) return ERR_ARG;
    if (*string == '+') ++string;
    if (!*string) return ERR_ARG;
    unsigned long long value = 0;
    for (; *string; ++string) {
        if (*string < '0' || *string > '9') return ERR_ARG;
        const unsigned digit = (unsigned)(*string - '0');
        if (value > (ULLONG_MAX - digit) / 10) return ERR_RANGE;
        value = value * 10 + digit;
    }
    *result = value; return OK;
}

static Status parse_double(const char *string, double *const result) {
    if (!string || !*string || !result) return ERR_ARG;
    char *end;
    double value = strtod(string, &end);
    if (*end || !isfinite(value) || value < 0.0) return ERR_ARG;
    *result = value; return OK;
}

static Status copy_name(const char *const text, char **const result) {
    if (!text || !*text || !result) return ERR_ARG;
    *result = NULL;
    const size_t length = strlen(text);
    if (length == SIZE_MAX) return ERR_RANGE;
    char *copy = malloc(length + 1);
    if (!copy) return ERR_MEM;
    memcpy(copy, text, length + 1); *result = copy;
    return OK;
}

typedef struct { char *name; double price; unsigned long long importance; } Item;
typedef struct { Item *items; size_t count; double total_price; unsigned long long total_importance; } Purchase;

static void free_items(Item *const items, const size_t count) {
    for (size_t i = 0; i < count; ++i) free(items[i].name);
    free(items);
}

static Status read_items(FILE *const input, Item **const result, size_t *const count, double *const budget) {
    if (!input || !result || !count || !budget) return ERR_ARG;
    *result = NULL; *count = 0; *budget = 0.0;
    char *line = NULL, *word[3]; bool exists;
    Status status = read_line(input, &line, &exists);
    unsigned long long item_count = 0;
    if (status == OK) status = exists ? fields(line, word, 2) : ERR_ARG;
    if (status == OK) status = parse_unsigned(word[0], &item_count);
    if (status == OK) status = parse_double(word[1], budget);
    free(line);
    if (status != OK) return status;
    if (!item_count || item_count > SIZE_MAX / sizeof(Item)) return ERR_RANGE;
    Item *items = calloc((size_t)item_count, sizeof(*items));
    if (!items) return ERR_MEM;
    for (size_t i = 0; i < (size_t)item_count && status == OK; ++i) {
        line = NULL;
        status = read_line(input, &line, &exists);
        if (status == OK) status = exists ? fields(line, word, 3) : ERR_ARG;
        if (status == OK) status = parse_double(word[1], &items[i].price);
        if (status == OK) status = parse_unsigned(word[2], &items[i].importance);
        if (status == OK) status = copy_name(word[0], &items[i].name);
        if (status == OK) {
            for (size_t j = 0; j < i; ++j)
                if (strcmp(items[i].name, items[j].name) == 0) status = ERR_ARG;
        }
        free(line);
    }
    if (status != OK) { free_items(items, (size_t)item_count); return status; }
    *result = items; *count = (size_t)item_count; return OK;
}

static Status choose_items(const Item *const items, const size_t count,
                           const double budget, Purchase *const result) {
    if (!items || !count || !result) return ERR_ARG;
    *result = (Purchase){NULL, 0, 0.0, 0};
    for (size_t i = 0; i < count; ++i) {
        if (!items[i].name || !*items[i].name || items[i].price <= 0.0) return ERR_ARG;
        for (const char *pointer = items[i].name; *pointer; ++pointer)
            if (isspace((unsigned char)*pointer)) return ERR_ARG;
        for (size_t j = 0; j < i; ++j)
            if (strcmp(items[i].name, items[j].name) == 0) return ERR_ARG;
    }
    double limit = 0.0;
    for (size_t i = 0; i < count; ++i)
        if (items[i].price <= budget) limit += items[i].price;
    if (limit > budget) limit = budget;
    if (!limit) return OK;
    const size_t precision = 100;
    const size_t columns = (size_t)(limit * precision) + 1;
    if (count + 1 > SIZE_MAX / columns / sizeof(unsigned long long) ||
        count > SIZE_MAX / sizeof(Item)) return ERR_RANGE;
    unsigned long long *table = calloc((count + 1) * columns, sizeof(*table));
    Item *selected = malloc(count * sizeof(*selected));
    if (!table || !selected) { free(table); free(selected); return ERR_MEM; }
    Status status = OK;
    for (size_t i = 1; i <= count && status == OK; ++i) {
        const Item item = items[i - 1];
        const size_t item_cost = (size_t)(item.price * precision);
        for (size_t current_budget = 0; current_budget <= (size_t)(limit * precision); ++current_budget) {
            unsigned long long best = table[(i - 1) * columns + current_budget];
            if (item_cost <= current_budget) {
                const unsigned long long before = table[(i - 1) * columns + current_budget - item_cost];
                if (before > ULLONG_MAX - item.importance) { status = ERR_RANGE; break; }
                const unsigned long long take = before + item.importance;
                if (take > best) best = take;
            }
            table[i * columns + current_budget] = best;
        }
    }
    size_t used = 0;
    double space = limit, total = 0.0;
    if (status == OK) {
        for (size_t i = count; i > 0; --i) {
            if (table[i * columns + (size_t)(space * precision)] !=
                table[(i - 1) * columns + (size_t)(space * precision)]) {
                selected[used++] = items[i - 1];
                space -= items[i - 1].price; total += items[i - 1].price;
            }
        }
        for (size_t i = 0; i < used / 2; ++i) {
            Item temporary = selected[i];
            selected[i] = selected[used - 1 - i];
            selected[used - 1 - i] = temporary;
        }
        result->total_importance = table[count * columns + (size_t)(limit * precision)];
        result->total_price = total; result->count = used;
    }
    free(table);
    if (status != OK || !used) free(selected);
    else result->items = selected;
    return status;
}

int main(void) {
    Item *items = NULL; size_t count = 0; double budget = 0.0;
    Purchase result = {NULL, 0, 0.0, 0};
    Status status = read_items(stdin, &items, &count, &budget);
    if (status == OK) status = choose_items(items, count, budget, &result);
    if (status == OK) {
        if (printf("TOTAL_PRICE %.2f\nTOTAL_IMPORTANCE %llu\nITEMS %zu\n",
                   result.total_price, result.total_importance, result.count) < 0) status = ERR_IO;
        for (size_t i = 0; i < result.count; ++i)
            if (puts(result.items[i].name) == EOF) status = ERR_IO;
    }
    free(result.items); free_items(items, count);
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    return status != OK || fflush(stdout) == EOF;
}