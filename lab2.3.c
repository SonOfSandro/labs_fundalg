#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO } Status;
typedef struct { size_t line, column; } Match;
typedef struct { const char *path; Match *matches; size_t count; } FileMatches;

static Status read_bytes(FILE *const input, unsigned char **const result, size_t *const length) {
    if (!input || !result || !length) return ERR_ARG;
    *result = NULL; *length = 0;
    size_t capacity = 256, used = 0;
    unsigned char *data = malloc(capacity);
    if (!data) return ERR_MEM;
    Status status = OK; int character;
    while ((character = fgetc(input)) != EOF) {
        if (used == capacity) {
            if (capacity > SIZE_MAX / 2) { status = ERR_RANGE; break; }
            unsigned char *next = realloc(data, capacity * 2);
            if (!next) { status = ERR_MEM; break; }
            data = next; capacity *= 2;
        }
        data[used++] = (unsigned char)character;
    }
    if (ferror(input)) status = ERR_IO;
    if (status != OK) { free(data); return status; }
    *result = data; *length = used; return OK;
}

static Status search_bytes(const unsigned char *const data, const size_t length,
                           const char *const pattern, Match **const result, size_t *const count) {
    if ((!data && length) || !pattern || !*pattern || !result || !count) return ERR_ARG;
    *result = NULL; *count = 0;
    const size_t pattern_length = strlen(pattern);
    Match *matches = NULL;
    size_t used = 0, capacity = 0, line = 1, column = 1;
    for (size_t i = 0; i < length; ++i) {
        size_t j = 0;
        if (pattern_length <= length - i)
            while (j < pattern_length && data[i + j] == (unsigned char)pattern[j]) ++j;
        if (j == pattern_length) {
            if (used == capacity) {
                if (capacity > SIZE_MAX / 2 / sizeof(*matches)) { free(matches); return ERR_RANGE; }
                const size_t next_capacity = capacity ? capacity * 2 : 16;
                Match *next = realloc(matches, next_capacity * sizeof(*matches));
                if (!next) { free(matches); return ERR_MEM; }
                matches = next; capacity = next_capacity;
            }
            matches[used++] = (Match){line, column};
        }
        if (data[i] == '\n') { if (line == SIZE_MAX) { free(matches); return ERR_RANGE; } ++line; column = 1; }
        else { if (column == SIZE_MAX) { free(matches); return ERR_RANGE; } ++column; }
    }
    *result = matches; *count = used; return OK;
}

static void free_results(FileMatches *const results, const size_t count) {
    for (size_t i = 0; i < count; ++i) free(results[i].matches);
    free(results);
}

static Status search_files(const char *const pattern, const int count, FileMatches **const result, ...) {
    if (!pattern || !*pattern || count <= 0 || !result) return ERR_ARG;
    *result = NULL;
    if ((size_t)count > SIZE_MAX / sizeof(FileMatches)) return ERR_RANGE;
    FileMatches *found = calloc((size_t)count, sizeof(*found));
    if (!found) return ERR_MEM;
    va_list arguments; va_start(arguments, result);
    Status status = OK;
    for (int i = 0; i < count && status == OK; ++i) {
        found[i].path = va_arg(arguments, const char *);
        if (!found[i].path || !*found[i].path) { status = ERR_ARG; break; }
        FILE *input = fopen(found[i].path, "rb");
        if (!input) { status = ERR_IO; break; }
        unsigned char *data = NULL; size_t length = 0;
        status = read_bytes(input, &data, &length);
        if (fclose(input) == EOF) status = ERR_IO;
        if (status == OK) status = search_bytes(data, length, pattern, &found[i].matches, &found[i].count);
        free(data);
    }
    va_end(arguments);
    if (status != OK) { free_results(found, (size_t)count); return status; }
    *result = found; return OK;
}

static Status print_matches(const FileMatches *const results, const size_t count) {
    if (!results && count) return ERR_ARG;
    for (size_t i = 0; i < count; ++i) {
        if (printf("%s: %zu matches\n", results[i].path, results[i].count) < 0) return ERR_IO;
        for (size_t j = 0; j < results[i].count; ++j)
            if (printf("  %zu:%zu\n", results[i].matches[j].line, results[i].matches[j].column) < 0) return ERR_IO;
    }
    return OK;
}

int main(int argc, char **argv) {
    Status status = OK;
    if (argc == 1) {
        FileMatches *result = NULL;
        status = search_files("aba", 2, &result, (const char *)"lab2/examples/search1.txt", (const char *)"lab2/examples/search2.txt");
        if (status == OK) { status = print_matches(result, 2); free_results(result, 2); }
    } else if (argc < 3) status = ERR_ARG;
    else for (int i = 2; i < argc && status == OK; ++i) {
        FileMatches *result = NULL;
        status = search_files(argv[1], 1, &result, (const char *)argv[i]);
        if (status == OK) { status = print_matches(result, 1); free_results(result, 1); }
    }
    if (status != OK) fprintf(stderr, "Error %d. Usage: %s pattern file [files...]\n", status, argv[0]);
    return status != OK || fflush(stdout) == EOF;
}