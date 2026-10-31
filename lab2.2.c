#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

typedef enum {
    STATUS_OK, STATUS_ERR_ARG, STATUS_ERR_FORMAT,
    STATUS_ERR_RANGE, STATUS_ERR_MEMORY, STATUS_ERR_IO
} Status;

typedef struct {
    unsigned int id;
    char *name, *surname;
    double salary;
} Employee;

static Status read_dynamic_line(FILE *const in, char **const out, bool *const has) {
    if (!in || !out || !has) return STATUS_ERR_ARG;
    *out = NULL; *has = false;
    size_t cap = 128, len = 0;
    char *buf = malloc(cap);
    if (!buf) return STATUS_ERR_MEMORY;
    int ch;
    while ((ch = fgetc(in)) != EOF) {
        *has = true;
        if (ch == '\n') break;
        if (len + 1 >= cap) {
            if (cap > SIZE_MAX / 2) { free(buf); return STATUS_ERR_RANGE; }
            cap *= 2;
            char *t = realloc(buf, cap);
            if (!t) { free(buf); return STATUS_ERR_MEMORY; }
            buf = t;
        }
        buf[len++] = (char)ch;
    }
    if (ferror(in)) { free(buf); return STATUS_ERR_IO; }
    buf[len] = '\0'; *out = buf;
    return STATUS_OK;
}

static Status split_fields(char *line, char **f, const size_t cnt) {
    if (!line || !f) return STATUS_ERR_ARG;
    size_t n = 0; char *p = line;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (n == cnt) return STATUS_ERR_FORMAT;
        f[n++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';
    }
    return n == cnt ? STATUS_OK : STATUS_ERR_FORMAT;
}

static Status parse_uint(const char *const s, unsigned int *const r) {
    if (!s || !*s || !r) return STATUS_ERR_ARG;
    unsigned long long t = 0;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return STATUS_ERR_FORMAT;
        t = t * 10 + (unsigned int)(s[i] - '0');
        if (t > UINT_MAX) return STATUS_ERR_RANGE;
    }
    *r = (unsigned int)t;
    return STATUS_OK;
}

static Status validate_name(const char *const s) {
    if (!s || !*s) return STATUS_ERR_ARG;
    for (size_t i = 0; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return STATUS_ERR_FORMAT;
    }
    return STATUS_OK;
}

static Status parse_salary(const char *const s, double *const r) {
    if (!s || !*s || !r) return STATUS_ERR_ARG;
    const char *p = s;
    bool dig = false, dot = false;
    while (*p && *p != 'e' && *p != 'E') {
        if (*p == '.') { if (dot) return STATUS_ERR_FORMAT; dot = true; }
        else if (*p >= '0' && *p <= '9') dig = true;
        else return STATUS_ERR_FORMAT;
        p++;
    }
    if (!dig) return STATUS_ERR_FORMAT;
    if (*p) {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!*p) return STATUS_ERR_FORMAT;
        bool edig = false;
        while (*p) {
            if (*p < '0' || *p > '9') return STATUS_ERR_FORMAT;
            edig = true; p++;
        }
        if (!edig) return STATUS_ERR_FORMAT;
    }
    char *end;
    double v = strtod(s, &end);
    if (*end || !isfinite(v) || v < 0.0) return STATUS_ERR_FORMAT;
    *r = v;
    return STATUS_OK;
}

static Status dup_str(const char *const src, char **const dst) {
    if (!src || !dst) return STATUS_ERR_ARG;
    size_t n = strlen(src);
    char *c = malloc(n + 1);
    if (!c) return STATUS_ERR_MEMORY;
    memcpy(c, src, n + 1); *dst = c;
    return STATUS_OK;
}

static void free_employees(Employee *const e, const size_t n) {
    if (!e) return;
    for (size_t i = 0; i < n; i++) { free(e[i].name); free(e[i].surname); }
    free(e);
}

static Status load_employees(FILE *const in, Employee **const out, size_t *const cnt) {
    if (!in || !out || !cnt) return STATUS_ERR_ARG;
    *out = NULL; *cnt = 0;
    size_t cap = 0, n = 0;
    Employee *arr = NULL;
    Status st = STATUS_OK;
    while (st == STATUS_OK) {
        char *line = NULL; bool has = false;
        st = read_dynamic_line(in, &line, &has);
        if (st != STATUS_OK || !has) { free(line); break; }
        char *f[4]; Employee e = {0, NULL, NULL, 0.0};
        st = split_fields(line, f, 4);
        if (st == STATUS_OK) st = parse_uint(f[0], &e.id);
        if (st == STATUS_OK) st = validate_name(f[1]);
        if (st == STATUS_OK) st = validate_name(f[2]);
        if (st == STATUS_OK) st = parse_salary(f[3], &e.salary);
        if (st == STATUS_OK) st = dup_str(f[1], &e.name);
        if (st == STATUS_OK) st = dup_str(f[2], &e.surname);
        if (st == STATUS_OK && n == cap) {
            size_t nc = cap ? cap * 2 : 16;
            if (nc > SIZE_MAX / sizeof(Employee)) st = STATUS_ERR_RANGE;
            else {
                Employee *t = realloc(arr, nc * sizeof(Employee));
                if (!t) st = STATUS_ERR_MEMORY;
                else { arr = t; cap = nc; }
            }
        }
        if (st == STATUS_OK) arr[n++] = e;
        else { free(e.name); free(e.surname); }
        free(line);
    }
    if (st != STATUS_OK) { free_employees(arr, n); return st; }
    *out = arr; *cnt = n;
    return STATUS_OK;
}

static int cmp_exact(const void *a, const void *b) {
    const Employee *ea = (const Employee *)a, *eb = (const Employee *)b;
    if (ea->salary < eb->salary) return -1;
    if (ea->salary > eb->salary) return 1;
    int c = strcmp(ea->surname, eb->surname);
    if (c) return c < 0 ? -1 : 1;
    c = strcmp(ea->name, eb->name);
    if (c) return c < 0 ? -1 : 1;
    return (ea->id > eb->id) - (ea->id < eb->id);
}

static int cmp_asc(const void *a, const void *b) { return cmp_exact(a, b); }
static int cmp_desc(const void *a, const void *b) { return cmp_exact(b, a); }

static Status save_employees(FILE *const out, const Employee *const e, const size_t n) {
    if (!out || (!e && n)) return STATUS_ERR_ARG;
    for (size_t i = 0; i < n; i++)
        if (fprintf(out, "%u %s %s %g\n", e[i].id, e[i].name, e[i].surname, e[i].salary) < 0)
            return STATUS_ERR_IO;
    return fflush(out) == EOF ? STATUS_ERR_IO : STATUS_OK;
}

int main(int argc, char *argv[]) {
    if (argc != 4 || !((argv[2][0] == '-' || argv[2][0] == '/') &&
        (argv[2][1] == 'a' || argv[2][1] == 'd') && !argv[2][2])) {
        fprintf(stderr, "Usage: %s <input> <-a|-d|/a|/d> <output>\n", argv[0]);
        return EXIT_FAILURE;
    }
    bool asc = argv[2][1] == 'a';
    FILE *in = fopen(argv[1], "r");
    if (!in) { fprintf(stderr, "Error: Cannot open input\n"); return EXIT_FAILURE; }
    Employee *emp = NULL; size_t cnt = 0;
    Status st = load_employees(in, &emp, &cnt);
    if (fclose(in) == EOF) st = STATUS_ERR_IO;
    if (st == STATUS_OK && cnt > 1)
        qsort(emp, cnt, sizeof(Employee), asc ? cmp_asc : cmp_desc);
    if (st == STATUS_OK) {
        FILE *out = fopen(argv[3], "w");
        if (!out) { fprintf(stderr, "Error: Cannot open output\n"); st = STATUS_ERR_IO; }
        else { st = save_employees(out, emp, cnt); if (fclose(out) == EOF && st == STATUS_OK) st = STATUS_ERR_IO; }
    }
    free_employees(emp, cnt);
    if (st != STATUS_OK) { fprintf(stderr, "Error: %d\n", st); return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}