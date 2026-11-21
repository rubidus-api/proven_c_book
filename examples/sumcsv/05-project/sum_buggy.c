/* 후행 쉼표 검사가 빠진 결함 판 --- 회귀 시험이 고치기 전에는 실패하는 것을 보이는 용도다. */
#include "sumcsv.h"

#include <stdckdint.h>

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static bool at_eol(const char *p)
{
    return *p == '\0' || *p == '\n' || (*p == '\r' && p[1] == '\n');
}

const char *sumcsv_err_str(sumcsv_err_t err)
{
    switch (err) {
    case SUMCSV_OK:              return "ok";
    case SUMCSV_ERR_EMPTY:       return "empty input";
    case SUMCSV_ERR_EMPTY_FIELD: return "empty field";
    case SUMCSV_ERR_BAD_NUMBER:  return "bad integer";
    case SUMCSV_ERR_RANGE:       return "integer out of range";
    case SUMCSV_ERR_OVERFLOW:    return "sum overflow";
    case SUMCSV_ERR_TRAILING:    return "trailing comma";
    }
    return "unknown error";
}

struct sumcsv_res sumcsv_sum_line(const char *line)
{
    struct sumcsv_res res = { .err = SUMCSV_ERR_EMPTY, .sum = 0, .pos = 0 };
    if (!line) return res;

    const char *p = skip_ws(line);
    if (at_eol(p)) return res;

    int64_t acc = 0;
    for (;;) {
        int64_t val = 0;
        const char *rest = nullptr;
        sumcsv_err_t err = sumcsv_parse_i64(p, &rest, &val);
        if (err != SUMCSV_OK) {
            res.err = err;
            res.pos = (size_t)(rest - line);
            return res;
        }
        if (ckd_add(&acc, acc, val)) {
            res.err = SUMCSV_ERR_OVERFLOW;
            res.pos = (size_t)(p - line);
            return res;
        }
        p = skip_ws(rest);
        if (*p == ',') {
            p++;
            if (at_eol(skip_ws(p))) {
                /* 결함: "10,20," 처럼 후행 쉼표 뒤가 비었으면 그냥 끝내 버린다 */
                res.err = SUMCSV_OK;
                res.sum = acc;
                res.pos = (size_t)(p - line);
                return res;
            }
            continue;
        }
        if (*p == '\r' && p[1] == '\n') p += 2;
        else if (*p == '\n') p++;
        if (*p == '\0') {
            res.err = SUMCSV_OK;
            res.sum = acc;
            res.pos = (size_t)(p - line);
            return res;
        }
        res.err = SUMCSV_ERR_BAD_NUMBER;
        res.pos = (size_t)(p - line);
        return res;
    }
}
