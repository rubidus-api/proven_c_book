/* 한 줄 전체의 구문과 필드 경계를 검사한다: 빈 입력 · 빈 필드 · 후행 쉼표 · 범위 초과 · 쓰레기 토큰 거부. */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    if (!line || *line == '\0') {
        *out_why = "empty input";
        return false;
    }
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
        acc += val;
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

static void print_case_label(const char *s)
{
    char buf[32];
    size_t j = 0;
    buf[j++] = '"';
    for (size_t i = 0; s[i] && j + 4 < sizeof buf; i++) {
        if (s[i] == '\n') { buf[j++] = '\\'; buf[j++] = 'n'; }
        else if (s[i] == '\r') { buf[j++] = '\\'; buf[j++] = 'r'; }
        else { buf[j++] = s[i]; }
    }
    buf[j++] = '"';
    buf[j] = '\0';
    printf("  %-26s", buf);
}

int main(void)
{
    const char *cases[] = {
        "10,20,30\n",
        "  -5 , +12 , 3 \r\n",
        "0",
        "",
        "   \n",
        ",10\n",
        "10,,30\n",
        "10,20,\n",
        "10,x,30\n",
        "10 20\n",
        "99999999999999999999\n",
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int64_t sum = -1;
        const char *why = "";
        print_case_label(cases[i]);
        if (sum_line(cases[i], &sum, &why)) {
            printf(" -> ok: %" PRId64 "\n", sum);
        } else {
            printf(" -> err: %s\n", why);
        }
    }
    return 0;
}
