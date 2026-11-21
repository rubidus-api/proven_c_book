/* 한 필드의 64비트 부호 있는 정수 해석 --- 공백 · 부호 · 범위 검사를 한곳에 둔다. */
#include "sumcsv.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

sumcsv_err_t sumcsv_parse_i64(const char *start, const char **out_rest,
                              int64_t *out_val)
{
    if (!start || !out_rest || !out_val) return SUMCSV_ERR_BAD_NUMBER;

    const char *p = start;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '\n' || (*p == '\r' && p[1] == '\n') || *p == ',') {
        *out_rest = p;
        return SUMCSV_ERR_EMPTY_FIELD;
    }

    const char *digits = p;
    if (*digits == '+' || *digits == '-') digits++;
    if (!isdigit((unsigned char)*digits)) {
        *out_rest = p;
        return SUMCSV_ERR_BAD_NUMBER;
    }

    errno = 0;
    char *end = nullptr;
    long long v = strtoll(p, &end, 10);
    if (end == p) {
        *out_rest = p;
        return SUMCSV_ERR_BAD_NUMBER;
    }
    if (errno == ERANGE) {
        *out_rest = p;
        return SUMCSV_ERR_RANGE;
    }
#if LLONG_MIN < INT64_MIN || LLONG_MAX > INT64_MAX
    if (v < INT64_MIN || v > INT64_MAX) {
        *out_rest = p;
        return SUMCSV_ERR_RANGE;
    }
#endif

    *out_val = (int64_t)v;
    *out_rest = end;
    return SUMCSV_OK;
}
