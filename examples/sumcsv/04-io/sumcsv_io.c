/* 긴 줄 잘림, 둘째 줄, 읽기 실패, 쓰기/닫기 실패를 모두 잡고 오류 시 stdout 을 비운다. */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdckdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LINE_CAP = 64 };

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static bool at_eol(const char *p)
{
    return *p == '\0' || *p == '\n' || (*p == '\r' && p[1] == '\n');
}

static bool parse_i64_field(const char *start, const char **out_rest,
                            int64_t *out_val, const char **out_why)
{
    const char *p = skip_ws(start);
    if (at_eol(p) || *p == ',') {
        *out_why = "empty field";
        return false;
    }
    const char *digits = p;
    if (*digits == '+' || *digits == '-') digits++;
    if (!isdigit((unsigned char)*digits)) {
        *out_why = "bad integer";
        return false;
    }

    errno = 0;
    char *end = nullptr;
    long long v = strtoll(p, &end, 10);
    if (end == p) {
        *out_why = "bad integer";
        return false;
    }
    if (errno == ERANGE) {
        *out_why = "integer out of range";
        return false;
    }
#if LLONG_MIN < INT64_MIN || LLONG_MAX > INT64_MAX
    if (v < INT64_MIN || v > INT64_MAX) {
        *out_why = "integer out of range";
        return false;
    }
#endif

    *out_val = (int64_t)v;
    *out_rest = end;
    return true;
}

static bool sum_line(const char *line, int64_t *out_sum, const char **out_why)
{
    const char *p = skip_ws(line);
    if (at_eol(p)) {
        *out_why = "empty input";
        return false;
    }

    int64_t acc = 0;
    for (;;) {
        int64_t val = 0;
        const char *rest = nullptr;
        if (!parse_i64_field(p, &rest, &val, out_why)) return false;
        if (ckd_add(&acc, acc, val)) {
            *out_why = "sum overflow";
            return false;
        }
        p = skip_ws(rest);
        if (*p == ',') {
            p++;
            if (at_eol(skip_ws(p))) {
                *out_why = "trailing comma";
                return false;
            }
            continue;
        }
        if (*p == '\r' && p[1] == '\n') p += 2;
        else if (*p == '\n') p++;
        if (*p == '\0') {
            *out_sum = acc;
            return true;
        }
        *out_why = "unexpected character";
        return false;
    }
}

static bool read_single_line(FILE *in, char *buf, size_t cap, const char **out_why)
{
    if (!fgets(buf, (int)cap, in)) {
        *out_why = ferror(in) ? "read error" : "empty input";
        return false;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        int extra = fgetc(in);
        if (ferror(in)) {
            *out_why = "read error";
            return false;
        }
        if (extra != EOF) {
            *out_why = "extra line";
            return false;
        }
        return true;
    }
    if (len == cap - 1) {
        int next = fgetc(in);
        if (ferror(in)) {
            *out_why = "read error";
            return false;
        }
        if (next != EOF) {
            *out_why = "line too long";
            return false;
        }
    }
    return true;
}

static bool emit_sum(FILE *out, int64_t sum, const char **out_why)
{
    if (fprintf(out, "%" PRId64 "\n", sum) < 0 || fflush(out) != 0 || ferror(out)) {
        *out_why = "write error";
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    bool test_write_fail = (argc == 2 && strcmp(argv[1], "--test-write-fail") == 0);
    if (argc > 1 && !test_write_fail) {
        fprintf(stderr, "sumcsv: unexpected argument\n");
        return 1;
    }

    char line[LINE_CAP];
    const char *why = "";
    if (!read_single_line(stdin, line, sizeof line, &why)) {
        fprintf(stderr, "sumcsv: %s\n", why);
        return 1;
    }

    int64_t sum = 0;
    if (!sum_line(line, &sum, &why)) {
        fprintf(stderr, "sumcsv: %s\n", why);
        return 1;
    }

    if (test_write_fail) {
        FILE *ro = fopen("/dev/null", "r");
        if (!ro) {
            fprintf(stderr, "sumcsv: write error\n");
            return 1;
        }
        bool ok = emit_sum(ro, sum, &why);
        fclose(ro);
        if (!ok) {
            fprintf(stderr, "sumcsv: %s\n", why);
            return 1;
        }
        return 0;
    }

    if (!emit_sum(stdout, sum, &why)) {
        fprintf(stderr, "sumcsv: %s\n", why);
        return 1;
    }
    return 0;
}
