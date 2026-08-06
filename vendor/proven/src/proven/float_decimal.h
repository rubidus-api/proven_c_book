#ifndef PROVEN_FLOAT_DECIMAL_H
#define PROVEN_FLOAT_DECIMAL_H

#include "proven/types.h"

/*
 * Internal float helpers shared by the scanner and formatter.
 * This is a private implementation detail, not a public API surface.
 */

typedef struct proven_u128_parts_t {
    proven_u64 lo;
    proven_u64 hi;
} proven_u128_parts_t;

typedef enum {
    PROVEN_FLOAT_PARSE_KIND_DECIMAL = 0,
    PROVEN_FLOAT_PARSE_KIND_INF,
    PROVEN_FLOAT_PARSE_KIND_NAN,
} proven_float_parse_kind_t;

typedef struct {
    proven_err_t err;
    proven_float_parse_kind_t kind;
    bool negative;
    proven_size_t consumed;
    bool has_nonzero_digit;
} proven_float_parse_result_t;

typedef struct {
    proven_u64 total_conversions;
    proven_u64 clinger_fast_path_hits;
    proven_u64 eisel_lemire_fast_path_hits;
    proven_u64 eisel_lemire_product_plan_hits;
    proven_u64 exact_fallback_hits;
} proven_float_decimal_stats_t;

proven_u32 proven_float_bits_f32(float value);
proven_u64 proven_float_bits_f64(double value);
proven_u128_parts_t proven_float_mul_u64_u64_to_u128(proven_u64 lhs, proven_u64 rhs);

/*
 * Big-integer division on little-endian base-2^64 limb arrays. Computes
 * quot = floor(num / den) and rem = num mod den. The caller provides output
 * buffers with capacity for at least nlen (quotient) and dlen (remainder)
 * limbs; the significant lengths are written to *qlen and *rlen. num and den
 * must each have at most 160 significant limbs and be trimmed (no leading-zero
 * limb). Returns false on division by zero or an over-capacity operand.
 * Exposed for testing the shared float decimal backend's division helper.
 */
bool proven_float_bigint_divmod_u64(const proven_u64 *num, proven_size_t nlen,
                                    const proven_u64 *den, proven_size_t dlen,
                                    proven_u64 *quot, proven_size_t *qlen,
                                    proven_u64 *rem, proven_size_t *rlen);

/*
 * Computes round-half-to-even(|value| * 10^scale_exp10) exactly and writes its
 * decimal digits (most significant first, no sign) to out, NUL-terminated.
 * Returns the digit count, or -1 on capacity overflow. value must be finite;
 * zero yields "0". Shared exact engine for the fixed-precision and scientific
 * float formatters.
 */
int proven_float_scaled_round_digits(double value, proven_i64 scale_exp10, char *out, proven_size_t out_cap);

/*
 * Rounds |value| to sig_digits significant decimal digits (round half to even),
 * writing exactly that many digits (most significant first) to out and setting
 * *decimal_exp to the power of ten of the leading digit. Returns sig_digits or -1
 * on overflow. value must be finite; zero yields all-zero digits, exponent 0.
 */
int proven_float_scaled_round_sig_digits(double value, int sig_digits, char *out, proven_size_t out_cap,
                                         proven_i64 *decimal_exp);

/*
 * Shortest round-trippable significant digits of |value| (round-to-nearest-ties-to
 * -even), most significant first, with *decimal_exp set to the power of ten of the
 * leading digit. Returns the digit count or -1. value must be finite; zero -> "0".
 */
int proven_float_shortest_digits(double value, char *out, proven_size_t out_cap, proven_i64 *decimal_exp);
int proven_float_shortest_digits_f32(float value, char *out, proven_size_t out_cap, proven_i64 *decimal_exp);

proven_float_parse_result_t proven_float_parse_ascii_token(const proven_u8 *input, proven_size_t len);
proven_err_t proven_float_convert_decimal(const proven_u8 *input, proven_size_t len, double *out);
/*
 * Internal test seam for observing conversion paths without process-global
 * state. When stats is non-NULL, counters accumulate into caller-owned storage.
 */
proven_err_t proven_float_convert_decimal_observed(const proven_u8 *input, proven_size_t len, double *out,
                                                   proven_float_decimal_stats_t *stats);
bool proven_float_normalize_scientific(double *abs_v, int *sci_exp);

#endif /* PROVEN_FLOAT_DECIMAL_H */
