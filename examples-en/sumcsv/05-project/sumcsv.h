#ifndef SUMCSV_H
#define SUMCSV_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    SUMCSV_OK = 0,
    SUMCSV_ERR_EMPTY,
    SUMCSV_ERR_EMPTY_FIELD,
    SUMCSV_ERR_BAD_NUMBER,
    SUMCSV_ERR_RANGE,
    SUMCSV_ERR_OVERFLOW,
    SUMCSV_ERR_TRAILING,
} sumcsv_err_t;

/* Result of summing one CSV line, paired with byte position on failure */
struct sumcsv_res {
    sumcsv_err_t err;
    int64_t      sum;
    size_t       pos;
};

const char *sumcsv_err_str(sumcsv_err_t err);

[[nodiscard]]
sumcsv_err_t sumcsv_parse_i64(const char *start, const char **out_rest,
                              int64_t *out_val);

[[nodiscard]]
struct sumcsv_res sumcsv_sum_line(const char *line);

#endif
