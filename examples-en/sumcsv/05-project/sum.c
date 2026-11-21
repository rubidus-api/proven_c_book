/* Sums one comma-separated line using ckd_add and rejects trailing commas or unexpected tokens. */
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
    if (at_eol(p)) {
        res.pos = (size_t)(p - line);
        return res;
    }

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
                res.err = SUMCSV_ERR_TRAILING;
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
