#if defined( MINGW32 ) && !defined(__USE_MINGW_ANSI_STDIO)
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <wchar.h>
#include <math.h>
#include <float.h>

typedef enum { OK, ERR_ARG, ERR_RANGE, ERR_MEM, ERR_IO } Status;

static Status parse_base(const char *string, int base, const bool upper, int *const result) {
    if (!string || !*string || !result) return ERR_ARG;
    if (base < 2 || base > 36) base = 10;
    const bool negative = *string == '-';
    if (*string == '+' || *string == '-') ++string;
    if (!*string) return ERR_ARG;
    const int limit = negative ? INT_MIN : -INT_MAX;
    int value = 0;
    for (; *string; ++string) {
        int digit = *string >= '0' && *string <= '9' ? *string - '0' : -1;
        if (upper && *string >= 'A' && *string <= 'Z') digit = *string - 'A' + 10;
        if (!upper && *string >= 'a' && *string <= 'z') digit = *string - 'a' + 10;
        if (digit < 0 || digit >= base) return ERR_ARG;
        if (value < (limit + digit) / base) return ERR_RANGE;
        value = value * base - digit;
    }
    *result = negative ? value : -value;
    return OK;
}

static int roman_digit(const int character) {
    switch (character) {
        case 'I': return 1; case 'V': return 5; case 'X': return 10;
        case 'L': return 50; case 'C': return 100; case 'D': return 500; case 'M': return 1000;
        default: return 0;
    }
}

static Status roman_text(const int value, char **const result) {
    if (!result) return ERR_ARG;
    *result = NULL;
    unsigned magnitude = value < 0 ? 0u - (unsigned)value : (unsigned)value;
    const size_t capacity = (size_t)(magnitude / 1000u) + 32;
    char *text = malloc(capacity);
    if (!text) return ERR_MEM;
    size_t position = 0;
    if (value < 0) text[position++] = '-';
    const unsigned values[] = {1000,900,500,400,100,90,50,40,10,9,5,4,1};
    const char *const symbols[] = {"M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"};
    if (!magnitude) text[position++] = 'N';
    for (size_t i = 0; i < sizeof(values)/sizeof(values[0]); ++i)
        while (magnitude >= values[i]) {
            for (size_t j = 0; symbols[i][j]; ++j) text[position++] = symbols[i][j];
            magnitude -= values[i];
        }
    text[position] = '\0'; *result = text; return OK;
}

static Status parse_roman(const char *text, int *const result) {
    if (!text || !*text || !result) return ERR_ARG;
    const char *original = text;
    const bool negative = *text == '-';
    if (negative) ++text;
    if (!strcmp(text,"N") && !negative) { *result = 0; return OK; }
    if (!*text) return ERR_ARG;
    long long sum = 0;
    const long long limit = negative ? -(long long)INT_MIN : INT_MAX;
    for (size_t i = 0; text[i]; ++i) {
        const int current = roman_digit((unsigned char)text[i]);
        const int next = roman_digit((unsigned char)text[i+1]);
        if (!current) return ERR_ARG;
        sum += current < next ? -current : current;
        if (sum > limit || sum < -limit) return ERR_RANGE;
    }
    if (sum < 0 || sum > limit) return ERR_RANGE;
    const int value = (int)(negative ? -sum : sum);
    char *canonical = NULL;
    Status status = roman_text(value, &canonical);
    if (status == OK && strcmp(original,canonical)) status = ERR_ARG;
    free(canonical);
    if (status == OK) *result = value;
    return status;
}

static Status parse_zeckendorf(const char *const text, unsigned *const result) {
    if (!text || !result) return ERR_ARG;
    const size_t length = strlen(text);
    if (!strcmp(text,"01")) { *result = 0; return OK; }
    if (length < 2 || text[length-1] != '1' || text[length-2] != '1') return ERR_ARG;
    unsigned a = 1, b = 2, sum = 0;
    for (size_t i = 0; i < length-1; ++i) {
        if (text[i] != '0' && text[i] != '1') return ERR_ARG;
        if (text[i] == '1') {
            if (i && text[i-1] == '1') return ERR_ARG;
            if (sum > UINT_MAX - a) return ERR_RANGE;
            sum += a;
        }
        if (i + 1 < length-1) {
            const unsigned next = a <= UINT_MAX-b ? a+b : 0;
            a=b; b=next;
            if (!a) return ERR_RANGE;
        }
    }
    *result = sum; return OK;
}

static Status push_char(char **const text, size_t *const length, size_t *const capacity, const int character) {
    if (*length + 1 >= *capacity) {
        if (*capacity > SIZE_MAX/2) return ERR_RANGE;
        const size_t next_size = *capacity ? *capacity*2 : 32;
        char *next = realloc(*text, next_size);
        if (!next) return ERR_MEM;
        *text=next; *capacity=next_size;
    }
    (*text)[(*length)++] = (char)character; (*text)[*length]='\0'; return OK;
}

static Status custom_word(FILE *const input, const char kind, const int base, const bool upper,
                          char **const result, size_t *const consumed, bool *const at_eof) {
    *result=NULL; *at_eof=false;
    int character;
    while ((character=fgetc(input)) != EOF && isspace((unsigned char)character)) {
        if (*consumed == INT_MAX) return ERR_RANGE;
        ++*consumed;
    }
    if (character == EOF) { *at_eof=true; return ferror(input) ? ERR_IO : OK; }
    size_t length=0, capacity=0; Status status=OK;
    while (character != EOF) {
        bool valid=false;
        if (kind=='R') valid=roman_digit(character)!=0 || (length==0 && (character=='-' || character=='N'));
        else if (kind=='Z') valid=character=='0' || character=='1';
        else {
            int digit=character >='0' && character <='9' ? character-'0' : -1;
            if (upper && character >='A' && character <='Z') digit=character-'A'+10;
            if (!upper && character >='a' && character <='z') digit=character-'a'+10;
            valid=(digit >=0 && digit <base) || (length==0 && (character=='+' || character=='-'));
        }
        if (!valid) { if (ungetc(character,input)==EOF) status=ERR_IO; break; }
        if (*consumed == INT_MAX) { status=ERR_RANGE; break; }
        ++*consumed;
        status=push_char(result, &length, &capacity, character);
        if (status!=OK) break;
        if (kind=='Z' && length >=2 && (*result)[length-1]=='1' && (*result)[length-2]=='1') break;
        character=fgetc(input);
    }
    if (ferror(input)) status=ERR_IO;
    if (status!=OK) { free(*result); *result=NULL; }
    return status;
}

static Status scan_spec(const char *const start, const char **const end, char length[3],
                        char *const conversion, bool *const suppress) {
    const char *pointer=start+1;
    *suppress= *pointer=='*'; if (*suppress) ++pointer;
    unsigned width=0; bool has_width=false;
    while (isdigit((unsigned char)*pointer)) {
        has_width=true;
        if (width > ((unsigned)INT_MAX-(unsigned)(*pointer-'0'))/10) return ERR_RANGE;
        width=width*10+(unsigned)(*pointer++-'0');
    }
    if (has_width && !width) return ERR_ARG;
    length[0]=length[1]=length[2]='\0';
    if (*pointer && strchr("hljztL", *pointer)) {
        length[0]=*pointer++;
        if ((length[0]=='h'||length[0]=='l') && *pointer==length[0]) length[1]=*pointer++;
    }
    if (!*pointer || !strchr("diouxXfFeEgGaAcspn[%", *pointer)) return ERR_ARG;
    *conversion=*pointer++;
    if (*conversion=='[') {
        if (*pointer=='^') ++pointer;
        if (*pointer==']') ++pointer;
        while (*pointer && *pointer!=']') ++pointer;
        if (*pointer!=']') return ERR_ARG;
        ++pointer;
    }
    if (strchr("diouxXn", *conversion) && length[0]=='L') return ERR_ARG;
    if (strchr("fFeEgGaA", *conversion) && length[0] && strcmp(length,"l") && strcmp(length,"L")) return ERR_ARG;
    if (strchr("cs[", *conversion) && length[0] && strcmp(length,"l")) return ERR_ARG;
    if (strchr("p%", *conversion) && length[0]) return ERR_ARG;
    if (*conversion=='%' && pointer-start!=2) return ERR_ARG;
    if (*conversion=='n' && (has_width || *suppress)) return ERR_ARG;
    *end=pointer; return OK;
}

static Status scan_count(va_list *const arguments, const char *const length, const size_t count) {
#define COUNT(TYPE, MAXIMUM) do { TYPE *pointer=va_arg(*arguments, TYPE *); if (!pointer) return ERR_ARG; \
    if (count >(size_t)(MAXIMUM)) { return ERR_RANGE; } *pointer=(TYPE)count; } while(0)
    if (!strcmp(length, "hh")) COUNT(signed char, SCHAR_MAX);
    else if (!strcmp(length, "h")) COUNT(short, SHRT_MAX);
    else if (!strcmp(length, "ll")) COUNT(long long, LLONG_MAX);
    else if (!strcmp(length, "l")) COUNT(long, LONG_MAX);
    else if (!strcmp(length, "j")) COUNT(intmax_t, INTMAX_MAX);
    else if (!strcmp(length, "z") || !strcmp(length, "t")) COUNT(ptrdiff_t, PTRDIFF_MAX);
    else COUNT(int, INT_MAX);
#undef COUNT
    return OK;
}

static Status scan_standard(FILE *const input, const char *const specification, const char *const length,
                            const char conversion, const bool suppress, va_list *const arguments,
                            int *const assigned, int *const consumed) {
    *consumed=-1;
    if (suppress || conversion=='%') { *assigned=fscanf(input, specification, consumed); return OK; }
#define READ(TYPE) do { TYPE *pointer=va_arg(*arguments, TYPE *); if (!pointer) return ERR_ARG; \
    *assigned=fscanf(input, specification, pointer, consumed); } while(0)
    if (strchr("di", conversion)) {
        if (!strcmp(length, "hh")) READ(signed char);
        else if (!strcmp(length, "h")) READ(short);
        else if (!strcmp(length, "ll")) READ(long long);
        else if (!strcmp(length, "l")) READ(long);
        else if (!strcmp(length, "j")) READ(intmax_t);
        else if (!strcmp(length, "z") || !strcmp(length, "t")) READ(ptrdiff_t);
        else READ(int);
    } else if (strchr("ouxX", conversion)) {
        if (!strcmp(length, "hh")) READ(unsigned char);
        else if (!strcmp(length, "h")) READ(unsigned short);
        else if (!strcmp(length, "ll")) READ(unsigned long long);
        else if (!strcmp(length, "l")) READ(unsigned long);
        else if (!strcmp(length, "j")) READ(uintmax_t);
        else if (!strcmp(length, "z") || !strcmp(length, "t")) READ(size_t);
        else READ(unsigned);
    } else if (strchr("fFeEgGaA", conversion)) {
        if (!strcmp(length, "L")) READ(long double);
        else if (!strcmp(length, "l")) READ(double);
        else READ(float);
    } else if (strchr("cs[", conversion)) {
        if (!strcmp(length, "l")) READ(wchar_t);
        else READ(char);
    } else if (conversion=='p') READ(void *);
    else return ERR_ARG;
#undef READ
    return OK;
}

static int scan_input(FILE *const input, const char *format, va_list *const arguments) {
    if (!input || !format || !arguments) return EOF;
    int assigned=0; size_t consumed=0;
    while (*format) {
        if (isspace((unsigned char)*format)) {
            while (isspace((unsigned char)*format)) ++format;
            int character;
            while ((character=fgetc(input))!=EOF && isspace((unsigned char)character)) {
                if (consumed==INT_MAX) return EOF;
                ++consumed;
            }
            if (character!=EOF && ungetc(character,input)==EOF) return EOF;
            if (ferror(input)) return EOF;
            continue;
        }
        if (*format!='%') {
            const int character=fgetc(input);
            if (character==EOF) return assigned ? assigned : EOF;
            if (character!=(unsigned char)*format) { if (ungetc(character,input)==EOF) return EOF; return assigned; }
            if (consumed==INT_MAX) return EOF;
            ++consumed; ++format; continue;
        }
        const char first=format[1], second=first ? format[2] : '\0';
        if ((first=='R' && second=='o') || (first=='Z' && second=='r') || (first=='C' && (second=='v'||second=='V'))) {
            int *integer=NULL; unsigned *natural=NULL; int base=10;
            if (first=='Z') natural=va_arg(*arguments, unsigned *);
            else integer=va_arg(*arguments, int *);
            if ((first=='Z' && !natural) || (first!='Z' && !integer)) return EOF;
            if (first=='C') { base=va_arg(*arguments, int); if (base <2||base >36) base=10; }
            char *word=NULL; bool at_eof;
            Status status=custom_word(input, first, base, second=='V', &word, &consumed, &at_eof);
            if (status!=OK) { free(word); return EOF; }
            if (!word) return at_eof && !assigned ? EOF : assigned;
            if (first=='R') status=parse_roman(word, integer);
            else if (first=='Z') status=parse_zeckendorf(word, natural);
            else status=parse_base(word, base, second=='V', integer);
            free(word);
            if (status!=OK) return assigned;
            ++assigned; format+=3; continue;
        }
        char length[3], conversion; const char *end; bool suppress;
        Status status=scan_spec(format, &end, length, &conversion, &suppress);
        if (status!=OK) return EOF;
        if (conversion=='n') {
            if (scan_count(arguments, length, consumed)!=OK) return EOF;
            format=end; continue;
        }
        const size_t count=(size_t)(end-format);
        if (count > SIZE_MAX-3) return EOF;
        char *specification=malloc(count+3);
        if (!specification) return EOF;
        memcpy(specification, format, count); memcpy(specification+count, "%n", 3);
        int current=0, used=-1;
        status=scan_standard(input, specification, length, conversion, suppress, arguments, &current, &used);
        free(specification);
        if (status!=OK || ferror(input)) return EOF;
        if (used < 0) return current==EOF && !assigned ? EOF : assigned;
        if ((size_t)used > (size_t)INT_MAX-consumed) return EOF;
        consumed+=(size_t)used;
        if (!suppress && conversion!='%') ++assigned;
        format=end;
    }
    return assigned;
}

static int overfscanf(FILE *const stream, const char *const format, ...) {
    if (!stream || !format) return EOF;
    va_list arguments;
    va_start(arguments, format); const int result=scan_input(stream, format, &arguments); va_end(arguments);
    return result;
}

static int oversscanf(const char *const text, const char *const format, ...) {
    if (!text || !format) return EOF;
    FILE *input=tmpfile();
    if (!input) return EOF;
    const size_t length=strlen(text);
    int result=EOF;
    if (fwrite(text, 1, length, input)==length && fflush(input)!=EOF && fseek(input, 0, SEEK_SET)==0) {
        va_list arguments;
        va_start(arguments, format); result=scan_input(input, format, &arguments); va_end(arguments);
    }
    if (fclose(input)==EOF) result=EOF;
    return result;
}

int main(void) {
    int roman=0, lower=0, upper=0, count=0; unsigned zeck=0; double real=0;
    const int number=oversscanf("XIV 0010011 ff FF 1.25", "%Ro %Zr %Cv %CV %lf%n",
                                &roman, &zeck, &lower, 16, &upper, 16, &real, &count);
    if (number!=5 || printf("%d %u %d %d %.2f; read %d bytes\n", roman, zeck, lower, upper, real, count) < 0) return 1;
    FILE *file=tmpfile();
    if (!file) return 1;
    bool failed=fputs("IX 42", file)==EOF || fflush(file)==EOF || fseek(file, 0, SEEK_SET)!=0;
    int value=0;
    if (!failed) failed=overfscanf(file, "%Ro %d", &roman, &value)!=2;
    if (fclose(file)==EOF) failed=true;
    if (!failed && printf("file: %d %d\n", roman, value) < 0) failed=true;
    return failed || fflush(stdout)==EOF;
}