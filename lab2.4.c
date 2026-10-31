#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>
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

typedef struct { char *name; size_t size; unsigned long long importance; } FileInfo;
typedef struct {
    FileInfo *files;
    size_t count, total_size;
    unsigned long long total_importance;
} Backup;

static void free_files(FileInfo *const files, const size_t count) {
    for (size_t i = 0; i < count; ++i) free(files[i].name);
    free(files);
}

static Status read_files(FILE *const input, FileInfo **const result, size_t *const count, size_t *const capacity) {
    if (!input || !result || !count || !capacity) return ERR_ARG;
    *result = NULL; *count = 0;
    char *line = NULL, *word[3]; bool exists;
    Status status = read_line(input, &line, &exists);
    unsigned long long file_count = 0, disk_capacity = 0;
    if (status == OK) status = exists ? fields(line, word, 2) : ERR_ARG;
    if (status == OK) status = parse_unsigned(word[0], &file_count);
    if (status == OK) status = parse_unsigned(word[1], &disk_capacity);
    free(line);
    if (status != OK) return status;
    if (!file_count || file_count > SIZE_MAX / sizeof(FileInfo) || disk_capacity > SIZE_MAX) return ERR_RANGE;
    FileInfo *files = calloc((size_t)file_count, sizeof(*files));
    if (!files) return ERR_MEM;
    for (size_t i = 0; i < (size_t)file_count && status == OK; ++i) {
        line = NULL;
        status = read_line(input, &line, &exists);
        if (status == OK) status = exists ? fields(line, word, 3) : ERR_ARG;
        unsigned long long file_size = 0;
        if (status == OK) status = parse_unsigned(word[1], &file_size);
        if (status == OK && (!file_size || file_size > SIZE_MAX)) status = ERR_RANGE;
        if (status == OK) status = parse_unsigned(word[2], &files[i].importance);
        if (status == OK) status = copy_name(word[0], &files[i].name);
        if (status == OK) {
            files[i].size = (size_t)file_size;
            for (size_t j = 0; j < i; ++j)
                if (strcmp(files[i].name, files[j].name) == 0) status = ERR_ARG;
        }
        free(line);
    }
    if (status != OK) { free_files(files, (size_t)file_count); return status; }
    *result = files; *count = (size_t)file_count; *capacity = (size_t)disk_capacity; return OK;
}

static Status choose_files(const FileInfo *const files, const size_t count,
                           const size_t capacity, Backup *const result) {
    if (!result) return ERR_ARG;
    *result = (Backup){NULL, 0, 0, 0};
    if (!files || !count) return ERR_ARG;
    for (size_t i = 0; i < count; ++i) {
        if (!files[i].name || !*files[i].name || !files[i].size) return ERR_ARG;
        for (const char *pointer = files[i].name; *pointer; ++pointer) if (isspace((unsigned char)*pointer)) return ERR_ARG;
        for (size_t j = 0; j < i; ++j) if (!strcmp(files[i].name, files[j].name)) return ERR_ARG;
    }
    size_t limit = 0;
    for (size_t i = 0; i < count; ++i) if (files[i].size <= capacity) {
        if (files[i].size > capacity - limit) limit = capacity;
        else limit += files[i].size;
    }
    if (!limit) return OK;
    if (count == SIZE_MAX || limit == SIZE_MAX) return ERR_RANGE;
    const size_t columns = limit + 1;
    if (count + 1 > SIZE_MAX / columns / sizeof(unsigned long long) ||
        count > SIZE_MAX / sizeof(FileInfo)) return ERR_RANGE;
    unsigned long long *table = calloc((count + 1) * columns, sizeof(*table));
    FileInfo *selected = malloc(count * sizeof(*selected));
    if (!table || !selected) { free(table); free(selected); return ERR_MEM; }
    Status status = OK;
    for (size_t i = 1; i <= count && status == OK; ++i) {
        const FileInfo file = files[i - 1];
        for (size_t current_capacity = 0; current_capacity <= limit; ++current_capacity) {
            unsigned long long best = table[(i - 1) * columns + current_capacity];
            if (file.size <= current_capacity) {
                const unsigned long long before = table[(i - 1) * columns + current_capacity - file.size];
                if (before > ULLONG_MAX - file.importance) { status = ERR_RANGE; break; }
                const unsigned long long take = before + file.importance;
                if (take > best) best = take;
            }
            table[i * columns + current_capacity] = best;
        }
    }
    size_t used = 0, space = limit, total = 0;
    if (status == OK) {
        for (size_t i = count; i > 0; --i) {
            if (table[i * columns + space] != table[(i - 1) * columns + space]) {
                selected[used++] = files[i - 1];
                space -= files[i - 1].size; total += files[i - 1].size;
            }
        }
        for (size_t i = 0; i < used / 2; ++i) {
            FileInfo temporary = selected[i]; selected[i] = selected[used - 1 - i]; selected[used - 1 - i] = temporary;
        }
        result->total_importance = table[count * columns + limit];
        result->total_size = total; result->count = used;
    }
    free(table);
    if (status != OK || !used) free(selected);
    else result->files = selected;
    return status;
}

int main(void) {
    FileInfo *files = NULL; size_t count = 0, capacity = 0;
    Backup result = {NULL, 0, 0, 0};
    Status status = read_files(stdin, &files, &count, &capacity);
    if (status == OK) status = choose_files(files, count, capacity, &result);
    if (status == OK) {
        if (printf("TOTAL_SIZE %zu\nTOTAL_IMPORTANCE %llu\nFILES %zu\n",
                   result.total_size, result.total_importance, result.count) < 0) status = ERR_IO;
        for (size_t i = 0; i < result.count; ++i) if (puts(result.files[i].name) == EOF) status = ERR_IO;
    }
    free(result.files); free_files(files, count);
    if (status != OK) fprintf(stderr, "Error: %d\n", status);
    return status != OK || fflush(stdout) == EOF;
}