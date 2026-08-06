#include "float_decimal.h"
#include "float_decimal_tables.h"
#include "proven/float_config.h"
#include <string.h>

static proven_u32 proven_float_bits_copy_u32(float value) {
    proven_u32 bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static proven_u64 proven_float_bits_copy_u64(double value) {
    proven_u64 bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

proven_u32 proven_float_bits_f32(float value) {
    return proven_float_bits_copy_u32(value);
}

proven_u64 proven_float_bits_f64(double value) {
    return proven_float_bits_copy_u64(value);
}

proven_u128_parts_t proven_float_mul_u64_u64_to_u128(proven_u64 lhs, proven_u64 rhs) {
    const proven_u64 lhs_lo = lhs & 0xffffffffu;
    const proven_u64 lhs_hi = lhs >> 32;
    const proven_u64 rhs_lo = rhs & 0xffffffffu;
    const proven_u64 rhs_hi = rhs >> 32;

    const proven_u64 p0 = lhs_lo * rhs_lo;
    const proven_u64 p1 = lhs_lo * rhs_hi;
    const proven_u64 p2 = lhs_hi * rhs_lo;
    const proven_u64 p3 = lhs_hi * rhs_hi;

    const proven_u64 middle = (p0 >> 32) + (p1 & 0xffffffffu) + (p2 & 0xffffffffu);

    proven_u128_parts_t out;
    out.lo = (p0 & 0xffffffffu) | (middle << 32);
    out.hi = p3 + (p1 >> 32) + (p2 >> 32) + (middle >> 32);
    return out;
}

/*
 * PROVEN_FLOAT_BIGINT_LIMBS, PROVEN_FLOAT_BIGINT_SCALE_HEADROOM, and
 * PROVEN_FLOAT_MAX_SIGNIFICAND_DIGITS come from proven/float_config.h so the big
 * integer capacity (and thus the exact-fallback stack footprint) is tunable at
 * build time. Any binary64 value or rounding midpoint has an exact decimal
 * expansion of at most 767 significant digits, which the default cap covers;
 * further nonzero digits only push the value strictly upward and are recorded by
 * significand_sticky instead of being stored, bounding the bigint work for
 * arbitrarily long inputs.
 */
typedef struct {
    proven_u64 limbs[PROVEN_FLOAT_BIGINT_LIMBS];
    proven_size_t len;
} proven_float_bigint_t;

typedef struct {
    bool negative;
    bool is_zero;
    bool mantissa_fits_u64;
    bool significand_sticky;
    proven_u64 mantissa_u64;
    proven_size_t significant_digits;
    proven_i64 exp10;
    proven_size_t digits_total;
    proven_size_t frac_digits;
    proven_size_t first_nonzero;
    proven_size_t last_nonzero;
    proven_float_bigint_t significand;
} proven_float_decimal_number_t;

typedef enum {
    PROVEN_FLOAT_FAST_PATH_UNSUPPORTED = 0,
    PROVEN_FLOAT_FAST_PATH_UNCERTAIN,
    PROVEN_FLOAT_FAST_PATH_SUCCESS,
} proven_float_fast_path_result_t;

typedef enum {
    PROVEN_FLOAT_EISEL_LEMIRE_CANDIDATE_PRODUCT = 0,
} proven_float_eisel_lemire_candidate_mode_t;

typedef struct {
    proven_float_eisel_lemire_candidate_mode_t mode;
    proven_u128_parts_t wide;
    proven_i64 exp2;
} proven_float_eisel_lemire_candidate_plan_t;

static proven_u8 proven_float_ascii_lower(proven_u8 ch) {
    if (ch >= (proven_u8)'A' && ch <= (proven_u8)'Z') {
        return (proven_u8)(ch - (proven_u8)'A' + (proven_u8)'a');
    }
    return ch;
}

static bool proven_float_match_keyword(const proven_u8 *input, proven_size_t len, const char *word, proven_size_t *matched_len) {
    proven_size_t i = 0;
    while (word[i] != '\0') {
        if (i >= len) {
            return false;
        }
        if (proven_float_ascii_lower(input[i]) != (proven_u8)word[i]) {
            return false;
        }
        ++i;
    }
    *matched_len = i;
    return true;
}

#define PROVEN_FLOAT_EXP10_PARSE_CLAMP 1000000ll

static proven_i64 proven_float_exp10_accumulate_clamped(proven_i64 current, proven_u8 digit) {
    const proven_i64 clamp = PROVEN_FLOAT_EXP10_PARSE_CLAMP;
    if (current >= clamp) {
        return clamp;
    }
    if (current > (clamp - (proven_i64)digit) / 10ll) {
        return clamp;
    }
    return current * 10ll + (proven_i64)digit;
}

/*
 * floor(k * log2(10)), in integers only.
 *
 * This used to go through `long double`, which is the one type in C whose width
 * is not the same on every target the library builds for: 80-bit on x86, 128-bit
 * on aarch64, and a plain 64-bit double on armhf and MSVC. The estimate happened
 * to come out identical on all of them - verified over the entire input range
 * this can see, k in [-1000900, 1000900] (the exp10 clamp plus the digit cap) -
 * so nothing was ever wrong. But a correctness-critical path had no business
 * depending on a type whose precision varies by platform, and on a soft-float
 * target the arithmetic pulls in libgcc routines for no reason.
 *
 * PROVEN_FLOAT_LOG2_10_Q40 is floor(log2(10) * 2^40). Over the full k range the
 * result is bit-identical to the exact floor of k * log2(10) - checked against
 * exact rational arithmetic for every k, not sampled - and |k * C| stays under
 * 3.66e18, well inside proven_i64.
 */
#define PROVEN_FLOAT_LOG2_10_Q40  ((proven_i64)3652498566964ll)
#define PROVEN_FLOAT_LOG2_10_SHIFT 40

static proven_i64 proven_float_floor_mul_log2_10(proven_i64 k) {
    const proven_i64 scale = (proven_i64)1 << PROVEN_FLOAT_LOG2_10_SHIFT;
    proven_i64 product = k * PROVEN_FLOAT_LOG2_10_Q40;
    if (product >= 0) {
        return product >> PROVEN_FLOAT_LOG2_10_SHIFT;
    }
    /* Floor, not truncate: C division rounds toward zero for negatives. */
    return -((-product + scale - 1) >> PROVEN_FLOAT_LOG2_10_SHIFT);
}

static void proven_float_decimal_binary_exp_bounds(const proven_float_decimal_number_t *decimal,
                                                   proven_size_t significant_digits,
                                                   proven_i64 *min_exp2_out,
                                                   proven_i64 *max_exp2_out) {
    proven_i64 digits = (proven_i64)significant_digits;
    proven_i64 k_min = decimal->exp10 + digits - 1;
    proven_i64 k_max = decimal->exp10 + digits;
    proven_i64 min_exp2 = proven_float_floor_mul_log2_10(k_min) - 2;
    proven_i64 max_exp2 = proven_float_floor_mul_log2_10(k_max) + 2;

    if (min_exp2 < -1074) {
        min_exp2 = -1074;
    }
    if (max_exp2 > 1023) {
        max_exp2 = 1023;
    }
    if (min_exp2 > max_exp2) {
        min_exp2 = -1074;
        max_exp2 = 1023;
    }

    *min_exp2_out = min_exp2;
    *max_exp2_out = max_exp2;
}

proven_float_parse_result_t proven_float_parse_ascii_token(const proven_u8 *input, proven_size_t len) {
    proven_float_parse_result_t out = {
        .err = PROVEN_ERR_INVALID_ARG,
        .kind = PROVEN_FLOAT_PARSE_KIND_DECIMAL,
        .negative = false,
        .consumed = 0,
        .has_nonzero_digit = false,
    };
    if (!input || len == 0) {
        return out;
    }

    proven_size_t cursor = 0;
    if (input[cursor] == (proven_u8)'-') {
        out.negative = true;
        ++cursor;
    } else if (input[cursor] == (proven_u8)'+') {
        ++cursor;
    }

    if (cursor >= len) {
        return out;
    }

    {
        proven_size_t matched = 0;
        if (proven_float_match_keyword(input + cursor, len - cursor, "infinity", &matched) ||
            proven_float_match_keyword(input + cursor, len - cursor, "inf", &matched)) {
            out.err = PROVEN_OK;
            out.kind = PROVEN_FLOAT_PARSE_KIND_INF;
            out.consumed = cursor + matched;
            return out;
        }
        if (proven_float_match_keyword(input + cursor, len - cursor, "nan", &matched)) {
            proven_size_t end = cursor + matched;
            if (end < len && input[end] == (proven_u8)'(') {
                ++end;
                while (end < len && input[end] != (proven_u8)')') {
                    ++end;
                }
                if (end < len && input[end] == (proven_u8)')') {
                    ++end;
                } else {
                    return out;
                }
            }
            out.err = PROVEN_OK;
            out.kind = PROVEN_FLOAT_PARSE_KIND_NAN;
            out.consumed = end;
            return out;
        }
    }

    bool found_digit = false;
    bool has_nonzero_digit = false;

    while (cursor < len && input[cursor] >= (proven_u8)'0' && input[cursor] <= (proven_u8)'9') {
        found_digit = true;
        if (input[cursor] != (proven_u8)'0') {
            has_nonzero_digit = true;
        }
        ++cursor;
    }

    if (cursor < len && input[cursor] == (proven_u8)'.') {
        ++cursor;
        while (cursor < len && input[cursor] >= (proven_u8)'0' && input[cursor] <= (proven_u8)'9') {
            found_digit = true;
            if (input[cursor] != (proven_u8)'0') {
                has_nonzero_digit = true;
            }
            ++cursor;
        }
    }

    if (!found_digit) {
        return out;
    }

    if (cursor < len && (input[cursor] == (proven_u8)'e' || input[cursor] == (proven_u8)'E')) {
        proven_size_t exp_anchor = cursor;
        ++cursor;

        if (cursor < len && (input[cursor] == (proven_u8)'+' || input[cursor] == (proven_u8)'-')) {
            ++cursor;
        }

        if (cursor >= len || input[cursor] < (proven_u8)'0' || input[cursor] > (proven_u8)'9') {
            /*
             * An `e`/`E` not followed by a valid exponent is not part of the
             * number. Match strtod: rewind to the `e` and accept the mantissa
             * parsed so far instead of rejecting the whole token.
             */
            cursor = exp_anchor;
        } else {
            while (cursor < len && input[cursor] >= (proven_u8)'0' && input[cursor] <= (proven_u8)'9') {
                ++cursor;
            }
        }
    }

    out.err = PROVEN_OK;
    out.kind = PROVEN_FLOAT_PARSE_KIND_DECIMAL;
    out.consumed = cursor;
    out.has_nonzero_digit = has_nonzero_digit;
    return out;
}

static int proven_float_clz_u64(proven_u64 value) {
    if (value == 0u) {
        return 64;
    }
    int count = 0;
    while ((value & 0x8000000000000000ull) == 0u) {
        value <<= 1;
        ++count;
    }
    return count;
}

static void proven_float_bigint_zero(proven_float_bigint_t *value) {
    if (!value) {
        return;
    }
    value->len = 0;
    memset(value->limbs, 0, sizeof value->limbs);
}

static void proven_float_bigint_trim(proven_float_bigint_t *value) {
    while (value->len > 0 && value->limbs[value->len - 1] == 0) {
        --value->len;
    }
}

static void proven_float_bigint_clear_prefix(proven_float_bigint_t *value, proven_size_t len) {
    if (!value) {
        return;
    }
    if (len > PROVEN_FLOAT_BIGINT_LIMBS) {
        len = PROVEN_FLOAT_BIGINT_LIMBS;
    }
    memset(value->limbs, 0, len * sizeof value->limbs[0]);
    value->len = 0;
}

static void proven_float_bigint_from_u64(proven_float_bigint_t *value, proven_u64 u64) {
    if (!value) {
        return;
    }
    value->len = 0;
    if (u64 != 0) {
        value->limbs[0] = u64;
        value->len = 1;
    }
}

static void proven_float_bigint_from_u128_parts(proven_float_bigint_t *value, proven_u128_parts_t u128) {
    if (!value) {
        return;
    }
    value->len = 0;
    if (u128.lo != 0u) {
        value->limbs[0] = u128.lo;
        value->len = 1u;
    }
    if (u128.hi != 0u) {
        value->limbs[value->len++] = u128.hi;
    }
}

static void proven_float_bigint_set_u64(proven_float_bigint_t *value, proven_u64 u64) {
    if (u64 != 0u) {
        value->limbs[0] = u64;
        value->len = 1u;
    } else {
        value->len = 0u;
    }
}

static void proven_float_bigint_set_u128_parts(proven_float_bigint_t *value, proven_u128_parts_t u128) {
    if (u128.lo != 0u) {
        value->limbs[0] = u128.lo;
        value->len = 1u;
    } else {
        value->len = 0u;
    }
    if (u128.hi != 0u) {
        value->limbs[value->len++] = u128.hi;
    }
}

static void proven_float_bigint_copy(proven_float_bigint_t *dst, const proven_float_bigint_t *src) {
    if (!dst || !src) {
        return;
    }
    dst->len = src->len;
    switch (src->len) {
        case 0u:
            break;
        case 1u:
            dst->limbs[0] = src->limbs[0];
            break;
        case 2u:
            dst->limbs[0] = src->limbs[0];
            dst->limbs[1] = src->limbs[1];
            break;
        case 3u:
            dst->limbs[0] = src->limbs[0];
            dst->limbs[1] = src->limbs[1];
            dst->limbs[2] = src->limbs[2];
            break;
        case 4u:
            dst->limbs[0] = src->limbs[0];
            dst->limbs[1] = src->limbs[1];
            dst->limbs[2] = src->limbs[2];
            dst->limbs[3] = src->limbs[3];
            break;
        default:
            memcpy(dst->limbs, src->limbs, src->len * sizeof dst->limbs[0]);
            break;
    }
}

static bool proven_float_bigint_mul_small(proven_float_bigint_t *value, proven_u32 mul) {
    if (value->len == 0 || mul == 1u) {
        return true;
    }
    if (mul == 0u) {
        proven_float_bigint_zero(value);
        return true;
    }

    proven_u64 carry = 0;
    for (proven_size_t i = 0; i < value->len; ++i) {
        proven_u128_parts_t product = proven_float_mul_u64_u64_to_u128(value->limbs[i], (proven_u64)mul);
        proven_u64 lo = product.lo + carry;
        proven_u64 hi = product.hi + (lo < product.lo ? 1ull : 0ull);
        value->limbs[i] = lo;
        carry = hi;
    }

    if (carry != 0) {
        if (value->len >= PROVEN_FLOAT_BIGINT_LIMBS) {
            return false;
        }
        value->limbs[value->len++] = carry;
    }
    return true;
}

static bool proven_float_bigint_mul_u64(proven_float_bigint_t *value, proven_u64 mul) {
    if (value->len == 0 || mul == 1u) {
        return true;
    }
    if (mul == 0u) {
        proven_float_bigint_zero(value);
        return true;
    }

    proven_u64 carry = 0;
    for (proven_size_t i = 0; i < value->len; ++i) {
        proven_u128_parts_t product = proven_float_mul_u64_u64_to_u128(value->limbs[i], mul);
        proven_u64 lo = product.lo + carry;
        proven_u64 hi = product.hi + (lo < product.lo ? 1ull : 0ull);
        value->limbs[i] = lo;
        carry = hi;
    }

    if (carry != 0) {
        if (value->len >= PROVEN_FLOAT_BIGINT_LIMBS) {
            return false;
        }
        value->limbs[value->len++] = carry;
    }
    return true;
}

static bool proven_float_bigint_add_small(proven_float_bigint_t *value, proven_u32 add) {
    if (value->len == 0) {
        if (add == 0u) {
            return true;
        }
        value->limbs[0] = (proven_u64)add;
        value->len = 1;
        return true;
    }

    proven_u64 carry = (proven_u64)add;
    for (proven_size_t i = 0; i < value->len && carry != 0; ++i) {
        proven_u64 next = value->limbs[i] + carry;
        carry = next < value->limbs[i] ? 1ull : 0ull;
        value->limbs[i] = next;
    }
    if (carry != 0) {
        if (value->len >= PROVEN_FLOAT_BIGINT_LIMBS) {
            return false;
        }
        value->limbs[value->len++] = carry;
    }
    return true;
}

/* dst += src. Used by the Dragon4 shortest digit generator. */
static bool proven_float_bigint_add(proven_float_bigint_t *dst, const proven_float_bigint_t *src) {
    proven_size_t max_len = dst->len > src->len ? dst->len : src->len;
    proven_u64 carry = 0;

    if (max_len > PROVEN_FLOAT_BIGINT_LIMBS) {
        return false;
    }
    for (proven_size_t i = 0; i < max_len; ++i) {
        proven_u64 lhs = i < dst->len ? dst->limbs[i] : 0ull;
        proven_u64 rhs = i < src->len ? src->limbs[i] : 0ull;
        proven_u64 sum = lhs + rhs;
        proven_u64 carry_out = sum < lhs ? 1ull : 0ull;
        sum += carry;
        if (sum < carry) {
            carry_out += 1ull;
        }
        dst->limbs[i] = sum;
        carry = carry_out;
    }
    dst->len = max_len;
    while (carry != 0) {
        if (dst->len >= PROVEN_FLOAT_BIGINT_LIMBS) {
            return false;
        }
        dst->limbs[dst->len++] = carry;
        carry = 0;
    }
    proven_float_bigint_trim(dst);
    return true;
}

static bool proven_float_bigint_shl_bits(proven_float_bigint_t *value, proven_i64 shift);

static bool proven_float_bigint_mul_factor(proven_float_bigint_t *value, const proven_float_bigint_t *factor) {
    proven_float_bigint_t product;

    if (!value || !factor) {
        return false;
    }
    if (value->len == 0 || factor->len == 0) {
        proven_float_bigint_zero(value);
        return true;
    }
    if (factor->len == 1u) {
        return proven_float_bigint_mul_u64(value, factor->limbs[0]);
    }
    if (value->len + factor->len > PROVEN_FLOAT_BIGINT_LIMBS) {
        return false;
    }
#if defined(__SIZEOF_INT128__)
    if (factor->len == 2u) {
        const unsigned __int128 f0 = factor->limbs[0];
        const unsigned __int128 f1 = factor->limbs[1];

        proven_float_bigint_clear_prefix(&product, value->len + 2u);
        for (proven_size_t i = 0; i < value->len; ++i) {
            unsigned __int128 a = value->limbs[i];
            unsigned __int128 acc0 = (unsigned __int128)product.limbs[i] + a * f0;
            unsigned __int128 acc1 = (unsigned __int128)product.limbs[i + 1u] + (acc0 >> 64) + a * f1;

            product.limbs[i] = (proven_u64)acc0;
            product.limbs[i + 1u] = (proven_u64)acc1;

            {
                unsigned __int128 carry = acc1 >> 64;
                proven_size_t k = i + 2u;
                while (carry != 0u) {
                    unsigned __int128 next = (unsigned __int128)product.limbs[k] + carry;
                    product.limbs[k] = (proven_u64)next;
                    carry = next >> 64;
                    ++k;
                }
            }
        }

        product.len = value->len + 2u;
        proven_float_bigint_trim(&product);
        *value = product;
        return true;
    }
#endif

    proven_float_bigint_clear_prefix(&product, value->len + factor->len);
    for (proven_size_t i = 0; i < value->len; ++i) {
        proven_u64 carry = 0;

        for (proven_size_t j = 0; j < factor->len; ++j) {
            proven_u128_parts_t mul = proven_float_mul_u64_u64_to_u128(value->limbs[i], factor->limbs[j]);
            proven_u64 acc = product.limbs[i + j] + mul.lo;
            proven_u64 carry_out = acc < product.limbs[i + j] ? 1ull : 0ull;

            acc += carry;
            if (acc < carry) {
                carry_out += 1ull;
            }

            product.limbs[i + j] = acc;
            carry = mul.hi + carry_out;
        }

        {
            proven_size_t k = i + factor->len;
            while (carry != 0u) {
                proven_u64 acc = product.limbs[k] + carry;
                carry = acc < product.limbs[k] ? 1ull : 0ull;
                product.limbs[k] = acc;
                ++k;
            }
        }
    }

    product.len = value->len + factor->len;
    proven_float_bigint_trim(&product);
    *value = product;
    return true;
}

static bool proven_float_bigint_copy_mul_factor(proven_float_bigint_t *dst, const proven_float_bigint_t *src,
                                                const proven_float_bigint_t *factor) {
    proven_float_bigint_t product;

    if (!dst || !src || !factor) {
        return false;
    }
    if (src->len == 0u || factor->len == 0u) {
        proven_float_bigint_zero(dst);
        return true;
    }
    if (src->len + factor->len > PROVEN_FLOAT_BIGINT_LIMBS) {
        return false;
    }
#if defined(__SIZEOF_INT128__)
    if (factor->len == 1u) {
        proven_u64 mul = factor->limbs[0];

        if (mul == 0u) {
            proven_float_bigint_zero(dst);
            return true;
        }
        product.len = 0u;
        {
            proven_u64 carry = 0u;
            for (proven_size_t i = 0; i < src->len; ++i) {
                proven_u128_parts_t prod = proven_float_mul_u64_u64_to_u128(src->limbs[i], mul);
                proven_u64 lo = prod.lo + carry;
                proven_u64 carry_out = prod.hi + (lo < prod.lo ? 1ull : 0ull);
                product.limbs[i] = lo;
                carry = carry_out;
            }
            if (carry != 0u) {
                product.limbs[src->len] = carry;
                product.len = src->len + 1u;
            } else {
                product.len = src->len;
            }
        }
        *dst = product;
        return true;
    }
    if (factor->len == 2u) {
        proven_float_bigint_clear_prefix(&product, src->len + 2u);
        const unsigned __int128 f0 = factor->limbs[0];
        const unsigned __int128 f1 = factor->limbs[1];

        for (proven_size_t i = 0; i < src->len; ++i) {
            unsigned __int128 a = src->limbs[i];
            unsigned __int128 acc0 = (unsigned __int128)product.limbs[i] + a * f0;
            unsigned __int128 acc1 = (unsigned __int128)product.limbs[i + 1u] + (acc0 >> 64) + a * f1;

            product.limbs[i] = (proven_u64)acc0;
            product.limbs[i + 1u] = (proven_u64)acc1;

            {
                unsigned __int128 carry = acc1 >> 64;
                proven_size_t k = i + 2u;
                while (carry != 0u) {
                    unsigned __int128 next = (unsigned __int128)product.limbs[k] + carry;
                    product.limbs[k] = (proven_u64)next;
                    carry = next >> 64;
                    ++k;
                }
            }
        }

        product.len = src->len + 2u;
        proven_float_bigint_trim(&product);
        *dst = product;
        return true;
    }
#endif
    proven_float_bigint_clear_prefix(&product, src->len + factor->len);
    for (proven_size_t i = 0; i < src->len; ++i) {
        proven_u64 carry = 0;

        for (proven_size_t j = 0; j < factor->len; ++j) {
            proven_u128_parts_t mul = proven_float_mul_u64_u64_to_u128(src->limbs[i], factor->limbs[j]);
            proven_u64 acc = product.limbs[i + j] + mul.lo;
            proven_u64 carry_out = acc < product.limbs[i + j] ? 1ull : 0ull;

            acc += carry;
            if (acc < carry) {
                carry_out += 1ull;
            }

            product.limbs[i + j] = acc;
            carry = mul.hi + carry_out;
        }

        {
            proven_size_t k = i + factor->len;
            while (carry != 0u) {
                proven_u64 acc = product.limbs[k] + carry;
                carry = acc < product.limbs[k] ? 1ull : 0ull;
                product.limbs[k] = acc;
                ++k;
            }
        }
    }

    product.len = src->len + factor->len;
    proven_float_bigint_trim(&product);
    *dst = product;
    return true;
}

static bool proven_float_bigint_mul_u64_factor(proven_float_bigint_t *dst, proven_u64 src, const proven_float_bigint_t *factor) {
    if (!dst || !factor) {
        return false;
    }
    if (src == 0u || factor->len == 0u) {
        proven_float_bigint_zero(dst);
        return true;
    }

    dst->len = 0u;
    {
        proven_u64 carry = 0u;
        proven_size_t i = 0u;

        for (; i < factor->len; ++i) {
            proven_u128_parts_t prod = proven_float_mul_u64_u64_to_u128(src, factor->limbs[i]);
            proven_u64 lo = prod.lo + carry;
            proven_u64 carry_out = lo < prod.lo ? 1ull : 0ull;
            dst->limbs[i] = lo;
            carry = prod.hi + carry_out;
        }

        if (carry != 0u) {
            dst->limbs[i] = carry;
            dst->len = factor->len + 1u;
        } else {
            dst->len = factor->len;
        }
    }
    return true;
}

static bool proven_float_bigint_shl_bits(proven_float_bigint_t *value, proven_i64 shift) {
    if (shift <= 0 || value->len == 0) {
        return true;
    }

    proven_size_t limb_shift = (proven_size_t)(shift / 64);
    unsigned bit_shift = (unsigned)(shift % 64);

    if (value->len + limb_shift + (bit_shift != 0u ? 1u : 0u) > PROVEN_FLOAT_BIGINT_LIMBS) {
        return false;
    }

    if (limb_shift != 0u) {
        memmove(&value->limbs[limb_shift], &value->limbs[0], value->len * sizeof value->limbs[0]);
        memset(&value->limbs[0], 0, limb_shift * sizeof value->limbs[0]);
        value->len += limb_shift;
    }

    if (bit_shift != 0u) {
        if (limb_shift == 0u && value->len <= 2u) {
            proven_u64 carry = 0;
            if (value->len >= 1u) {
                proven_u64 next_carry = value->limbs[0] >> (64u - bit_shift);
                value->limbs[0] = (value->limbs[0] << bit_shift) | carry;
                carry = next_carry;
            }
            if (value->len == 2u) {
                proven_u64 next_carry = value->limbs[1] >> (64u - bit_shift);
                value->limbs[1] = (value->limbs[1] << bit_shift) | carry;
                carry = next_carry;
            }
            if (carry != 0u) {
                value->limbs[value->len++] = carry;
            }
            return true;
        }

        proven_u64 carry = 0;
        for (proven_size_t i = limb_shift; i < value->len; ++i) {
            proven_u64 next_carry = value->limbs[i] >> (64u - bit_shift);
            value->limbs[i] = (value->limbs[i] << bit_shift) | carry;
            carry = next_carry;
        }
        if (carry != 0u) {
            if (value->len >= PROVEN_FLOAT_BIGINT_LIMBS) {
                return false;
            }
            value->limbs[value->len++] = carry;
        }
    }

    return true;
}

static int proven_float_bigint_cmp(const proven_float_bigint_t *lhs, const proven_float_bigint_t *rhs) {
    if (lhs->len < rhs->len) {
        return -1;
    }
    if (lhs->len > rhs->len) {
        return 1;
    }
    for (proven_size_t i = lhs->len; i > 0; --i) {
        proven_u64 a = lhs->limbs[i - 1];
        proven_u64 b = rhs->limbs[i - 1];
        if (a < b) {
            return -1;
        }
        if (a > b) {
            return 1;
        }
    }
    return 0;
}

static int proven_float_bigint_cmp_shift_left(const proven_float_bigint_t *lhs, proven_size_t shift, const proven_float_bigint_t *rhs) {
    proven_size_t lhs_len = 0u;
    proven_size_t rhs_len = 0u;
    proven_size_t max_len = 0u;
    proven_size_t i = 0u;
    proven_size_t limb_shift = shift / 64u;
    unsigned bit_shift = (unsigned)(shift % 64u);
    proven_u64 top = 0u;

    if (!lhs || !rhs) {
        return -1;
    }
    if (lhs->len == 0u || rhs->len == 0u || shift == 0u) {
        return proven_float_bigint_cmp(lhs, rhs);
    }

    lhs_len = lhs->len + limb_shift;
    if (bit_shift != 0u) {
        top = lhs->limbs[lhs->len - 1u] >> (64u - bit_shift);
        if (top != 0u) {
            ++lhs_len;
        }
    }
    rhs_len = rhs->len;

    if (lhs_len < rhs_len) {
        return -1;
    }
    if (lhs_len > rhs_len) {
        return 1;
    }

    max_len = lhs_len;
    if (bit_shift == 0u) {
        for (i = max_len; i > 0u; --i) {
            /* Positions below limb_shift are the zero padding introduced by the
             * left shift; comparing them against rhs is what decides ties when
             * all higher limbs are equal. Reading lhs there would underflow. */
            proven_u64 a = (i - 1u >= limb_shift) ? lhs->limbs[i - 1u - limb_shift] : 0u;
            proven_u64 b = rhs->limbs[i - 1u];
            if (a < b) {
                return -1;
            }
            if (a > b) {
                return 1;
            }
        }
        return 0;
    }

    if (top != 0u) {
        proven_u64 b = rhs->limbs[max_len - 1u];
        if (top < b) {
            return -1;
        }
        if (top > b) {
            return 1;
        }
        --max_len;
    }

    if (max_len > limb_shift + 1u) {
        for (i = max_len; i > limb_shift + 1u; --i) {
            proven_u64 a = (lhs->limbs[i - 1u - limb_shift] << bit_shift) |
                           (lhs->limbs[i - 2u - limb_shift] >> (64u - bit_shift));
            proven_u64 b = rhs->limbs[i - 1u];
            if (a < b) {
                return -1;
            }
            if (a > b) {
                return 1;
            }
        }
    }

    {
        proven_u64 a = lhs->limbs[0] << bit_shift;
        proven_u64 b = rhs->limbs[limb_shift];

        if (a < b) {
            return -1;
        }
        if (a > b) {
            return 1;
        }
    }

    {
        proven_u64 lower_nonzero = 0u;

        for (i = limb_shift; i > 0u; --i) {
            lower_nonzero |= rhs->limbs[i - 1u];
        }
        if (lower_nonzero != 0u) {
            return -1;
        }
    }

    return 0;
}

/* Number of 32-bit limbs that back a bigint of PROVEN_FLOAT_BIGINT_LIMBS u64s. */
#define PROVEN_FLOAT_BIGINT_LIMBS32 (PROVEN_FLOAT_BIGINT_LIMBS * 2u)

static int proven_float_nlz_u32(proven_u32 value) {
    int count = 0;
    if (value == 0u) {
        return 32;
    }
    while ((value & 0x80000000u) == 0u) {
        value <<= 1;
        ++count;
    }
    return count;
}

/* Pack a base-2^64 bigint into base-2^32 limbs (little-endian). Returns the
 * number of significant 32-bit limbs (trailing zeros trimmed). */
static proven_size_t proven_float_bigint_to_u32(const proven_float_bigint_t *value, proven_u32 *out) {
    proven_size_t n = 0;
    for (proven_size_t i = 0; i < value->len; ++i) {
        out[2u * i] = (proven_u32)(value->limbs[i] & 0xffffffffu);
        out[2u * i + 1u] = (proven_u32)(value->limbs[i] >> 32);
    }
    n = value->len * 2u;
    while (n > 0u && out[n - 1u] == 0u) {
        --n;
    }
    return n;
}

/* Rebuild a base-2^64 bigint from base-2^32 limbs (little-endian, len32 of them). */
static void proven_float_bigint_from_u32(proven_float_bigint_t *value, const proven_u32 *in, proven_size_t len32) {
    proven_size_t len64 = (len32 + 1u) / 2u;
    proven_float_bigint_zero(value);
    for (proven_size_t i = 0; i < len64; ++i) {
        proven_u64 lo = in[2u * i];
        proven_u64 hi = (2u * i + 1u < len32) ? in[2u * i + 1u] : 0u;
        value->limbs[i] = lo | (hi << 32);
    }
    value->len = len64;
    proven_float_bigint_trim(value);
}

/*
 * Knuth Algorithm D long division on base-2^32 limbs (after Hacker's Delight
 * "divmnu"). Computes q = floor(u / v) and r = u mod v, where u has m limbs and
 * v has n >= 1 limbs (v[n-1] != 0). q receives m-n+1 limbs, r receives n limbs.
 * Requires m >= n. The normalization shift is applied with 64-bit arithmetic so
 * a zero shift never triggers an out-of-range 32-bit shift.
 */
static void proven_float_bigint_divmnu32(proven_u32 *q, proven_u32 *r,
                                         const proven_u32 *u, const proven_u32 *v,
                                         proven_size_t m, proven_size_t n) {
    const proven_u64 base = 0x100000000ull; /* 2^32 */
    proven_u32 un[PROVEN_FLOAT_BIGINT_LIMBS32 + 2u];
    proven_u32 vn[PROVEN_FLOAT_BIGINT_LIMBS32 + 1u];
    int s;
    proven_size_t i;
    proven_size_t j;

    if (n == 1u) {
        proven_u64 rem = 0u;
        for (j = m; j > 0u; --j) {
            proven_u64 cur = rem * base + u[j - 1u];
            q[j - 1u] = (proven_u32)(cur / v[0]);
            rem = cur - (proven_u64)q[j - 1u] * v[0];
        }
        if (r) {
            r[0] = (proven_u32)rem;
        }
        return;
    }

    s = proven_float_nlz_u32(v[n - 1u]);
    for (i = n - 1u; i > 0u; --i) {
        proven_u64 hi = (proven_u64)v[i] << s;
        proven_u64 lo = s ? ((proven_u64)v[i - 1u] >> (32 - s)) : 0u;
        vn[i] = (proven_u32)(hi | lo);
    }
    vn[0] = (proven_u32)((proven_u64)v[0] << s);

    un[m] = (proven_u32)(s ? ((proven_u64)u[m - 1u] >> (32 - s)) : 0u);
    for (i = m - 1u; i > 0u; --i) {
        proven_u64 hi = (proven_u64)u[i] << s;
        proven_u64 lo = s ? ((proven_u64)u[i - 1u] >> (32 - s)) : 0u;
        un[i] = (proven_u32)(hi | lo);
    }
    un[0] = (proven_u32)((proven_u64)u[0] << s);

    for (j = m - n + 1u; j > 0u; --j) {
        proven_size_t jj = j - 1u;
        proven_u64 num = (proven_u64)un[jj + n] * base + un[jj + n - 1u];
        proven_u64 qhat = num / vn[n - 1u];
        proven_u64 rhat = num - qhat * vn[n - 1u];
        proven_i64 k;
        proven_i64 t;

        while (qhat >= base || qhat * vn[n - 2u] > base * rhat + un[jj + n - 2u]) {
            --qhat;
            rhat += vn[n - 1u];
            if (rhat >= base) {
                break;
            }
        }

        k = 0;
        for (i = 0; i < n; ++i) {
            proven_u64 p = qhat * vn[i];
            t = (proven_i64)un[i + jj] - k - (proven_i64)(p & 0xffffffffu);
            un[i + jj] = (proven_u32)t;
            k = (proven_i64)(p >> 32) - (t >> 32);
        }
        t = (proven_i64)un[jj + n] - k;
        un[jj + n] = (proven_u32)t;

        q[jj] = (proven_u32)qhat;
        if (t < 0) {
            --q[jj];
            k = 0;
            for (i = 0; i < n; ++i) {
                proven_u64 sum = (proven_u64)un[i + jj] + vn[i] + (proven_u64)k;
                un[i + jj] = (proven_u32)sum;
                k = (proven_i64)(sum >> 32);
            }
            un[jj + n] = (proven_u32)((proven_u64)un[jj + n] + (proven_u64)k);
        }
    }

    if (r) {
        for (i = 0; i < n; ++i) {
            proven_u64 lo = (proven_u64)un[i] >> s;
            proven_u64 hi = s ? ((proven_u64)un[i + 1u] << (32 - s)) : 0u;
            r[i] = (proven_u32)(lo | hi);
        }
    }
}

/*
 * Computes quot = floor(num / den) and rem = num mod den for base-2^64 bigints.
 * Either output may be null. Returns false on division by zero. The quotient and
 * remainder always fit because they are no larger than num.
 */
static bool proven_float_bigint_divmod(const proven_float_bigint_t *num,
                                       const proven_float_bigint_t *den,
                                       proven_float_bigint_t *quot,
                                       proven_float_bigint_t *rem) {
    proven_u32 u[PROVEN_FLOAT_BIGINT_LIMBS32 + 1u];
    proven_u32 v[PROVEN_FLOAT_BIGINT_LIMBS32 + 1u];
    proven_u32 q[PROVEN_FLOAT_BIGINT_LIMBS32 + 1u];
    proven_u32 r[PROVEN_FLOAT_BIGINT_LIMBS32 + 1u];
    proven_size_t m;
    proven_size_t n;

    if (!num || !den) {
        return false;
    }
    n = proven_float_bigint_to_u32(den, v);
    if (n == 0u) {
        return false; /* division by zero */
    }
    m = proven_float_bigint_to_u32(num, u);

    if (m < n) {
        /* num < den: quotient 0, remainder = num. */
        if (quot) {
            proven_float_bigint_zero(quot);
        }
        if (rem) {
            proven_float_bigint_copy(rem, num);
        }
        return true;
    }

    proven_float_bigint_divmnu32(q, r, u, v, m, n);

    if (quot) {
        proven_float_bigint_from_u32(quot, q, m - n + 1u);
    }
    if (rem) {
        proven_float_bigint_from_u32(rem, r, n);
    }
    return true;
}

bool proven_float_bigint_divmod_u64(const proven_u64 *num, proven_size_t nlen,
                                    const proven_u64 *den, proven_size_t dlen,
                                    proven_u64 *quot, proven_size_t *qlen,
                                    proven_u64 *rem, proven_size_t *rlen) {
    proven_float_bigint_t bnum;
    proven_float_bigint_t bden;
    proven_float_bigint_t bquot;
    proven_float_bigint_t brem;
    proven_size_t i;

    if (!num || !den || !quot || !qlen || !rem || !rlen) {
        return false;
    }
    if (nlen > PROVEN_FLOAT_BIGINT_LIMBS || dlen > PROVEN_FLOAT_BIGINT_LIMBS) {
        return false;
    }

    proven_float_bigint_zero(&bnum);
    proven_float_bigint_zero(&bden);
    for (i = 0; i < nlen; ++i) {
        bnum.limbs[i] = num[i];
    }
    bnum.len = nlen;
    proven_float_bigint_trim(&bnum);
    for (i = 0; i < dlen; ++i) {
        bden.limbs[i] = den[i];
    }
    bden.len = dlen;
    proven_float_bigint_trim(&bden);

    if (!proven_float_bigint_divmod(&bnum, &bden, &bquot, &brem)) {
        return false;
    }

    for (i = 0; i < bquot.len; ++i) {
        quot[i] = bquot.limbs[i];
    }
    *qlen = bquot.len;
    for (i = 0; i < brem.len; ++i) {
        rem[i] = brem.limbs[i];
    }
    *rlen = brem.len;
    return true;
}

static bool proven_float_bigint_copy_mul_factor(proven_float_bigint_t *dst, const proven_float_bigint_t *src,
                                                const proven_float_bigint_t *factor);

/*
 * Multiplies value by an exact 5^exp10 using exponentiation by squaring, so the
 * cost grows with log(exp10) big multiplies rather than exp10 single-limb ones.
 * This matters for the formatter's scientific path at extreme exponents.
 */
/* 5^0 .. 5^27: 5^27 is the largest power of five that fits in a u64. Multiplying by
 * whole chunks of 27 turns a 350-step loop into thirteen single-limb multiplies. */
static const proven_u64 proven_float_pow5_u64[28] = {
    1ull, 5ull, 25ull, 125ull, 625ull, 3125ull, 15625ull, 78125ull, 390625ull, 1953125ull, 9765625ull, 48828125ull, 244140625ull, 1220703125ull, 6103515625ull, 30517578125ull, 152587890625ull, 762939453125ull, 3814697265625ull, 19073486328125ull, 95367431640625ull, 476837158203125ull, 2384185791015625ull, 11920928955078125ull, 59604644775390625ull, 298023223876953125ull, 1490116119384765625ull, 7450580596923828125ull
};
#define PROVEN_FLOAT_POW5_U64_MAX_E 27

static bool proven_float_bigint_mul_pow5(proven_float_bigint_t *value, proven_i64 exp10) {
    proven_i64 e = exp10;

    if (e <= 0) {
        return true;
    }

    /*
     * Multiply by 5^27 at a time.
     *
     * This used to multiply by 5, one limb-multiply per unit of exponent, for e <= 64 -
     * and above that it ran exponentiation-by-squaring over full big integers, copying
     * the whole thing at every step. Both are unnecessary: 5^27 is the largest power of
     * five that fits in a u64, so any exponent can be consumed 27 at a time with the same
     * single-limb multiply. 5^350 - the top of the range the exact fallback needs - costs
     * thirteen multiplies instead of 350, or instead of nine squarings of a growing
     * bigint. It matters because the exact tier is on the correctness path for every
     * boundary-adjacent decimal, and it had just become the only way to build 5^q.
     */
    while (e > PROVEN_FLOAT_POW5_U64_MAX_E) {
        if (!proven_float_bigint_mul_u64(value, proven_float_pow5_u64[PROVEN_FLOAT_POW5_U64_MAX_E])) {
            return false;
        }
        e -= PROVEN_FLOAT_POW5_U64_MAX_E;
    }
    return proven_float_bigint_mul_u64(value, proven_float_pow5_u64[e]);
}

static bool proven_float_bigint_build_pow5_cached(proven_i64 exp10, proven_float_bigint_t *out) {
    proven_size_t q = 0;
    proven_u128_parts_t wide = {0};

    if (!out || exp10 < 0) {
        return false;
    }

    q = (proven_size_t)exp10;
    out->len = 0u;

    if (q <= PROVEN_FLOAT_CACHED_POW5_U128_MAX_Q) {
        proven_float_bigint_from_u128_parts(out, (proven_u128_parts_t){
            .lo = proven_float_cached_pow5_u128[q].lo,
            .hi = proven_float_cached_pow5_u128[q].hi,
        });
        return true;
    }

    /*
     * Everything above the exact table is built by multiplication, not looked up.
     *
     * It used to take proven_float_cached_pow5_scaled_u128[q] - the Eisel-Lemire table -
     * and shift it left. That table holds a 128-bit mantissa ROUNDED to 128 bits, and
     * 5^q is odd, so `entry << shift` is exactly 5^q only when shift is 0. For every
     * 56 <= q <= 350 it was off, by about 1e-38 relative:
     *
     *     q=55  table ...078124   exact ...078125
     *
     * This is the fallback whose entire job is to be EXACT. It is the thing that decides,
     * bit for bit, which way a decimal sitting on a rounding boundary goes - and it was
     * comparing against a corrupted power of five. Every exact halfway value in that
     * window rounded the same direction instead of to even (up for a negative exponent,
     * down for a positive one), and a differential run against glibc found 2,923 of them.
     * The library's central promise - correctly rounded, ties to even, bit-identical to a
     * correct strtod - was false for any input with 20 or more significant digits in that
     * exponent range.
     *
     * The bigint multiply below is slower. It is also right, and this path is the slow
     * path by construction: Clinger and Eisel-Lemire already answer the common cases.
     */
    (void)wide;

    out->limbs[0] = 1u;
    out->len = 1u;
    return proven_float_bigint_mul_pow5(out, (proven_i64)q);
}

static double proven_float_from_bits(proven_u64 bits) {
    double value = 0.0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void proven_float_f64_to_scaled(proven_u64 bits, proven_u64 *mantissa, proven_i64 *exp2) {
    proven_u64 frac = bits & 0x000fffffffffffffull;
    proven_u64 exp = (bits >> 52) & 0x7ffull;

    if (bits == 0) {
        *mantissa = 0;
        *exp2 = -1074;
    } else if (exp == 0) {
        *mantissa = frac;
        *exp2 = -1074;
    } else {
        *mantissa = (1ull << 52) | frac;
        *exp2 = (proven_i64)exp - 1075;
    }
}

/* Upper bound on 18-digit chunks needed to print a full-capacity big integer. */
#define PROVEN_FLOAT_DECIMAL_CHUNKS ((PROVEN_FLOAT_BIGINT_LIMBS * 20u) / 18u + 2u)

/*
 * Writes the decimal digits of a big integer, most significant first, to out
 * (NUL-terminated, no sign). Returns the digit count, or -1 if out is too small.
 * A zero value yields "0". Digits are extracted 18 at a time via division by
 * 10^18 (a single-limb divisor, so the fast division path is used).
 */
static int proven_float_bigint_to_decimal(const proven_float_bigint_t *value, char *out, proven_size_t out_cap) {
    proven_u64 chunks[PROVEN_FLOAT_DECIMAL_CHUNKS];
    proven_float_bigint_t work;
    proven_float_bigint_t divisor;
    proven_float_bigint_t quot;
    proven_float_bigint_t rem;
    proven_size_t nchunks = 0;
    proven_size_t pos = 0;
    proven_size_t i;

    if (!out || out_cap == 0u) {
        return -1;
    }
    if (value->len == 0u) {
        if (out_cap < 2u) {
            return -1;
        }
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }

    proven_float_bigint_set_u64(&divisor, 1000000000000000000ull); /* 10^18 */
    proven_float_bigint_copy(&work, value);
    while (work.len != 0u) {
        if (nchunks >= PROVEN_FLOAT_DECIMAL_CHUNKS) {
            return -1;
        }
        if (!proven_float_bigint_divmod(&work, &divisor, &quot, &rem)) {
            return -1;
        }
        chunks[nchunks++] = (rem.len != 0u) ? rem.limbs[0] : 0u;
        proven_float_bigint_copy(&work, &quot);
    }

    /* Most significant chunk: no leading zeros. */
    {
        char tmp[20];
        proven_size_t tlen = 0;
        proven_u64 v = chunks[nchunks - 1u];
        if (v == 0u) {
            tmp[tlen++] = '0';
        } else {
            char rev[20];
            proven_size_t rl = 0;
            while (v != 0u) {
                rev[rl++] = (char)('0' + (int)(v % 10u));
                v /= 10u;
            }
            while (rl > 0u) {
                tmp[tlen++] = rev[--rl];
            }
        }
        if (pos + tlen >= out_cap) {
            return -1;
        }
        for (i = 0; i < tlen; ++i) {
            out[pos++] = tmp[i];
        }
    }
    /* Remaining chunks: exactly 18 zero-padded digits each. */
    for (i = nchunks - 1u; i > 0u; --i) {
        proven_u64 v = chunks[i - 1u];
        char tmp[18];
        proven_size_t d;
        for (d = 18u; d > 0u; --d) {
            tmp[d - 1u] = (char)('0' + (int)(v % 10u));
            v /= 10u;
        }
        if (pos + 18u >= out_cap) {
            return -1;
        }
        for (d = 0; d < 18u; ++d) {
            out[pos++] = tmp[d];
        }
    }

    if (pos >= out_cap) {
        return -1;
    }
    out[pos] = '\0';
    return (int)pos;
}

/*
 * Computes Q = round-half-to-even(|value| * 10^scale_exp10) as an exact integer
 * and writes its decimal digits (most significant first, no sign) to out. Returns
 * the digit count, or -1 on capacity overflow. value must be finite. A value of
 * zero yields "0". This is the exact engine shared by the fixed-precision and
 * scientific formatters.
 */
int proven_float_scaled_round_digits(double value, proven_i64 scale_exp10, char *out, proven_size_t out_cap) {
    proven_u64 bits = proven_float_bits_f64(value) & 0x7fffffffffffffffull;
    proven_u64 m = 0;
    proven_i64 e2 = 0;
    proven_float_bigint_t num;
    proven_float_bigint_t den;
    proven_float_bigint_t quot;
    proven_float_bigint_t rem;
    proven_i64 twoexp;
    int cmp;
    proven_u64 q_low;

    proven_float_f64_to_scaled(bits, &m, &e2);
    if (m == 0u) {
        if (!out || out_cap < 2u) {
            return -1;
        }
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }

    /* |value| * 10^F = m * 5^max(F,0) / 5^max(-F,0) * 2^(e2 + F). */
    proven_float_bigint_set_u64(&num, m);
    proven_float_bigint_set_u64(&den, 1u);
    if (scale_exp10 > 0) {
        if (!proven_float_bigint_mul_pow5(&num, scale_exp10)) {
            return -1;
        }
    } else if (scale_exp10 < 0) {
        if (!proven_float_bigint_mul_pow5(&den, -scale_exp10)) {
            return -1;
        }
    }

    twoexp = e2 + scale_exp10;
    if (twoexp > 0) {
        if (!proven_float_bigint_shl_bits(&num, twoexp)) {
            return -1;
        }
    } else if (twoexp < 0) {
        if (!proven_float_bigint_shl_bits(&den, -twoexp)) {
            return -1;
        }
    }

    if (!proven_float_bigint_divmod(&num, &den, &quot, &rem)) {
        return -1;
    }

    /* Round half to even: compare 2*rem against den. */
    if (!proven_float_bigint_shl_bits(&rem, 1)) {
        return -1;
    }
    cmp = proven_float_bigint_cmp(&rem, &den);
    q_low = (quot.len != 0u) ? quot.limbs[0] : 0u;
    if (cmp > 0 || (cmp == 0 && (q_low & 1u) != 0u)) {
        if (!proven_float_bigint_add_small(&quot, 1u)) {
            return -1;
        }
    }

    return proven_float_bigint_to_decimal(&quot, out, out_cap);
}

/*
 * Rounds |value| to exactly sig_digits significant decimal digits (round half to
 * even) and writes those digits (most significant first, no point/sign) to out.
 * Sets *decimal_exp to the power of ten of the leading digit, i.e.
 * value ~= out[0].out[1..] x 10^(*decimal_exp). Returns sig_digits, or -1 on
 * overflow. value must be finite; zero yields all-zero digits with exponent 0.
 * Shared exact engine for the scientific formatter.
 */
static int proven_float_compare_bigints_with_exp2(const proven_float_bigint_t *lhs_int, proven_i64 lhs_exp2,
                                                  const proven_float_bigint_t *rhs_int, proven_i64 rhs_exp2);

/*
 * Returns the sign of (|value| - 10^k): >0 if |value| > 10^k, 0 if equal, <0 if
 * less. Exact (big-integer) comparison. m/e2 describe |value| = m * 2^e2.
 */
static int proven_float_cmp_value_pow10(proven_u64 m, proven_i64 e2, proven_i64 k) {
    proven_float_bigint_t lhs;
    proven_float_bigint_t rhs;

    proven_float_bigint_set_u64(&lhs, m);
    if (k < 0) {
        if (!proven_float_bigint_mul_pow5(&lhs, -k)) {
            return 1; /* overflow: treat |value| as larger */
        }
    }
    proven_float_bigint_set_u64(&rhs, 1u);
    if (k > 0) {
        if (!proven_float_bigint_mul_pow5(&rhs, k)) {
            return -1;
        }
    }
    /* |value| - 10^k has the sign of (m*5^max(-k,0))*2^e2 - 5^max(k,0)*2^k. */
    return proven_float_compare_bigints_with_exp2(&lhs, e2, &rhs, k);
}

int proven_float_scaled_round_sig_digits(double value, int sig_digits, char *out, proven_size_t out_cap,
                                         proven_i64 *decimal_exp) {
    proven_u64 bits = proven_float_bits_f64(value) & 0x7fffffffffffffffull;
    proven_u64 m = 0;
    proven_i64 e2 = 0;
    int bitlen;
    proven_i64 approxlog2;
    proven_i64 prod;
    proven_i64 k;
    int iter;
    int nd;

    if (!out || !decimal_exp || sig_digits < 1 || out_cap < (proven_size_t)sig_digits + 1u) {
        return -1;
    }

    proven_float_f64_to_scaled(bits, &m, &e2);
    if (m == 0u) {
        int i;
        for (i = 0; i < sig_digits; ++i) {
            out[i] = '0';
        }
        out[sig_digits] = '\0';
        *decimal_exp = 0;
        return sig_digits;
    }

    /* Seed floor(log10(|value|)) from the binary exponent (1233/4096 ~= log10(2)). */
    bitlen = 64 - proven_float_clz_u64(m);
    approxlog2 = e2 + (proven_i64)bitlen - 1;
    prod = approxlog2 * 1233;
    k = prod / 4096;
    if (prod < 0 && (prod % 4096) != 0) {
        k -= 1;
    }

    /* Correct k to the exact unrounded exponent: 10^k <= |value| < 10^(k+1). */
    for (iter = 0; iter < 6; ++iter) {
        if (proven_float_cmp_value_pow10(m, e2, k) < 0) {
            k -= 1;
        } else if (proven_float_cmp_value_pow10(m, e2, k + 1) >= 0) {
            k += 1;
        } else {
            break;
        }
    }

    /* Round to sig_digits at the now-exact leading position. The result has
     * sig_digits digits, or sig_digits+1 when rounding carries to 10^sig_digits. */
    nd = proven_float_scaled_round_digits(value, (proven_i64)(sig_digits - 1) - k, out, out_cap);
    if (nd < 0) {
        return -1;
    }
    if (nd == sig_digits) {
        *decimal_exp = k;
        return sig_digits;
    }
    if (nd == sig_digits + 1) {
        /* out == "1" followed by sig_digits zeros; keep the first sig_digits and
         * bump the exponent. */
        out[sig_digits] = '\0';
        *decimal_exp = k + 1;
        return sig_digits;
    }
    return -1;
}

/*
 * Shortest round-trippable significant digits of |value| (Burger-Dybvig / Dragon4,
 * free-format, round-to-nearest-ties-to-even). Writes the digits (most significant
 * first, no sign/point) to out and sets *decimal_exp to the power of ten of the
 * leading digit. Returns the digit count or -1 on error/overflow. value must be
 * finite; zero yields "0" with exponent 0. Exact big-integer arithmetic, single
 * pass, no re-parsing.
 */

/* ---- Grisu3 fast shortest path (Loitsch / double-conversion fast-dtoa) ---- */

typedef struct {
    proven_u64 f;
    int e;
} proven_diy_fp_t;

/* (x.f * y.f) high 64 bits, rounded; e = x.e + y.e + 64. */
static proven_diy_fp_t proven_diy_fp_mul(proven_diy_fp_t x, proven_diy_fp_t y) {
    proven_u128_parts_t p = proven_float_mul_u64_u64_to_u128(x.f, y.f);
    proven_diy_fp_t r;
    r.f = p.hi + (p.lo >> 63); /* round to nearest via bit 63 of the low half */
    r.e = x.e + y.e + 64;
    return r;
}

static proven_diy_fp_t proven_diy_fp_normalize(proven_diy_fp_t x) {
    int c = proven_float_clz_u64(x.f);
    if (c >= 64) {          /* x.f == 0: a left shift by the type width is UB.
                               Mirror the masked result (f stays 0) without it. */
        x.f = 0;
        x.e -= 64;
        return x;
    }
    x.f <<= c;
    x.e -= c;
    return x;
}

static void proven_grisu_biggest_power_ten(proven_u32 number, int number_bits,
                                           proven_u32 *power, int *exponent_plus_one) {
    static const proven_u32 small_powers[] = {
        0u, 1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u,
        100000000u, 1000000000u,
    };
    int guess = ((number_bits + 1) * 1233) >> 12;
    guess++;
    if (number < small_powers[guess]) {
        guess--;
    }
    *power = small_powers[guess];
    *exponent_plus_one = guess;
}

static bool proven_grisu_round_weed(char *buffer, int length, proven_u64 distance_too_high_w,
                                    proven_u64 unsafe_interval, proven_u64 rest,
                                    proven_u64 ten_kappa, proven_u64 unit) {
    proven_u64 small_distance = distance_too_high_w - unit;
    proven_u64 big_distance = distance_too_high_w + unit;

    while (rest < small_distance &&
           unsafe_interval - rest >= ten_kappa &&
           (rest + ten_kappa < small_distance ||
            small_distance - rest >= rest + ten_kappa - small_distance)) {
        buffer[length - 1]--;
        rest += ten_kappa;
    }
    if (rest < big_distance &&
        unsafe_interval - rest >= ten_kappa &&
        (rest + ten_kappa < big_distance ||
         big_distance - rest > rest + ten_kappa - big_distance)) {
        return false;
    }
    return (2u * unit <= rest) && (rest <= unsafe_interval - 4u * unit);
}

static bool proven_grisu_digit_gen(proven_diy_fp_t low, proven_diy_fp_t w, proven_diy_fp_t high,
                                   char *buffer, int *length, int *kappa) {
    proven_u64 unit = 1u;
    proven_diy_fp_t too_low = { low.f - unit, low.e };
    proven_diy_fp_t too_high = { high.f + unit, high.e };
    proven_u64 unsafe_interval = too_high.f - too_low.f;
    proven_diy_fp_t one = { (proven_u64)1 << (-w.e), w.e };
    proven_u32 integrals = (proven_u32)(too_high.f >> (-one.e));
    proven_u64 fractionals = too_high.f & (one.f - 1u);
    proven_u32 divisor;
    int divisor_exp_plus_one;
    proven_u64 dist = too_high.f - w.f;

    proven_grisu_biggest_power_ten(integrals, 64 - (-one.e), &divisor, &divisor_exp_plus_one);
    *kappa = divisor_exp_plus_one;
    *length = 0;
    while (*kappa > 0) {
        int digit = (int)(integrals / divisor);
        buffer[*length] = (char)('0' + digit);
        (*length)++;
        integrals %= divisor;
        (*kappa)--;
        {
            proven_u64 rest = ((proven_u64)integrals << (-one.e)) + fractionals;
            if (rest < unsafe_interval) {
                return proven_grisu_round_weed(buffer, *length, dist, unsafe_interval, rest,
                                               (proven_u64)divisor << (-one.e), unit);
            }
        }
        divisor /= 10u;
    }
    for (;;) {
        int digit;
        fractionals *= 10u;
        unit *= 10u;
        unsafe_interval *= 10u;
        digit = (int)(fractionals >> (-one.e));
        buffer[*length] = (char)('0' + digit);
        (*length)++;
        fractionals &= one.f - 1u;
        (*kappa)--;
        if (fractionals < unsafe_interval) {
            return proven_grisu_round_weed(buffer, *length, dist * unit, unsafe_interval,
                                           fractionals, one.f, unit);
        }
    }
}

/*
 * Grisu3 shortest digits for |value| = f * 2^e, with pow2_boundary = 2^(p-1) and
 * min_e the smallest exponent. Returns the digit count and sets *decimal_exp (the
 * power of ten of the leading digit), or -1 when Grisu3 cannot prove the result is
 * shortest (the caller then falls back to the exact Dragon4 path).
 */
static int proven_float_shortest_digits_grisu(proven_u64 f, proven_i64 e, proven_u64 pow2_boundary,
                                              proven_i64 min_e, char *out, proven_size_t out_cap,
                                              proven_i64 *decimal_exp) {
    proven_diy_fp_t v = { f, (int)e };
    proven_diy_fp_t w;
    proven_diy_fp_t m_plus;
    proven_diy_fp_t m_minus;
    proven_diy_fp_t ten_mk;
    proven_diy_fp_t scaled_w;
    proven_diy_fp_t scaled_mp;
    proven_diy_fp_t scaled_mm;
    bool lower_closer = (f == pow2_boundary) && (e != min_e);
    int min_exp;
    int kk;
    int index;
    int mk;
    int length = 0;
    int kappa = 0;
    char buf[24];

    if (out_cap < 19u) {
        return -1;
    }

    m_plus = proven_diy_fp_normalize((proven_diy_fp_t){ (v.f << 1) + 1u, v.e - 1 });
    if (lower_closer) {
        m_minus = (proven_diy_fp_t){ (v.f << 2) - 1u, v.e - 2 };
    } else {
        m_minus = (proven_diy_fp_t){ (v.f << 1) - 1u, v.e - 1 };
    }
    m_minus.f <<= (m_minus.e - m_plus.e);
    m_minus.e = m_plus.e;
    w = proven_diy_fp_normalize(v);

    min_exp = -60 - (m_plus.e + 64);
    {
        double dk = (double)(min_exp + 63) * 0.30102999566398114;
        kk = (int)dk;
        if ((double)kk < dk) {
            kk++;
        }
    }
    index = (348 + kk - 1) / (int)PROVEN_FLOAT_GRISU_DECIMAL_DISTANCE + 1;
    if (index < 0 || index >= (int)PROVEN_FLOAT_GRISU_POWER_COUNT) {
        return -1;
    }
    ten_mk.f = proven_float_grisu_powers[index].f;
    ten_mk.e = proven_float_grisu_powers[index].e;
    mk = proven_float_grisu_powers[index].k;

    scaled_w = proven_diy_fp_mul(w, ten_mk);
    scaled_mp = proven_diy_fp_mul(m_plus, ten_mk);
    scaled_mm = proven_diy_fp_mul(m_minus, ten_mk);

    if (!proven_grisu_digit_gen(scaled_mm, scaled_w, scaled_mp, buf, &length, &kappa)) {
        return -1;
    }
    if ((proven_size_t)length + 1u > out_cap) {
        return -1;
    }
    {
        int i;
        for (i = 0; i < length; ++i) {
            out[i] = buf[i];
        }
        out[length] = '\0';
    }
    /* trailing-digit decimal exponent is (-mk + kappa); leading is + (length-1). */
    *decimal_exp = (proven_i64)(-mk + kappa) + (proven_i64)(length - 1);
    return length;
}

static int proven_float_shortest_digits_core(proven_u64 f, proven_i64 e, proven_u64 pow2_boundary,
                                             proven_i64 min_e, char *out, proven_size_t out_cap,
                                             proven_i64 *decimal_exp) {
    proven_float_bigint_t R;
    proven_float_bigint_t S;
    proven_float_bigint_t mp;
    proven_float_bigint_t mm;
    proven_float_bigint_t tmp;
    proven_float_bigint_t q;
    proven_float_bigint_t rem;
    bool even;
    bool boundary_closer;
    int bitlen;
    proven_i64 fl2;
    proven_i64 prod;
    proven_i64 floorlog10;
    proven_i64 k;
    proven_size_t pos = 0;

    if (!out || !decimal_exp || out_cap < 2u) {
        return -1;
    }
    if (f == 0u) {
        out[0] = '0';
        out[1] = '\0';
        *decimal_exp = 0;
        return 1;
    }

    even = (f & 1u) == 0u;
    boundary_closer = (f == pow2_boundary) && (e != min_e);

    /* Step 1: set up R, S, m+ (mp), m- (mm) as exact scaled boundaries. */
    proven_float_bigint_set_u64(&R, f);
    proven_float_bigint_set_u64(&S, 1u);
    proven_float_bigint_set_u64(&mp, 1u);
    proven_float_bigint_set_u64(&mm, 1u);
    if (e >= 0) {
        if (!boundary_closer) {
            if (!proven_float_bigint_shl_bits(&R, e + 1)) return -1;
            proven_float_bigint_set_u64(&S, 2u);
            if (!proven_float_bigint_shl_bits(&mp, e)) return -1;
            if (!proven_float_bigint_shl_bits(&mm, e)) return -1;
        } else {
            if (!proven_float_bigint_shl_bits(&R, e + 2)) return -1;
            proven_float_bigint_set_u64(&S, 4u);
            if (!proven_float_bigint_shl_bits(&mp, e + 1)) return -1;
            if (!proven_float_bigint_shl_bits(&mm, e)) return -1;
        }
    } else {
        if (!boundary_closer) {
            if (!proven_float_bigint_shl_bits(&R, 1)) return -1;
            if (!proven_float_bigint_shl_bits(&S, 1 - e)) return -1;
        } else {
            if (!proven_float_bigint_shl_bits(&R, 2)) return -1;
            if (!proven_float_bigint_shl_bits(&S, 2 - e)) return -1;
            proven_float_bigint_set_u64(&mp, 2u);
        }
    }

    /* Step 2: estimate k = ceil(log10(value)) (1233/4096 ~= log10(2)). */
    bitlen = 64 - proven_float_clz_u64(f);
    fl2 = e + (proven_i64)bitlen - 1;
    prod = fl2 * 1233;
    floorlog10 = (prod >= 0) ? (prod / 4096) : -(((-prod) + 4095) / 4096);
    k = floorlog10 + 1;

    /* Step 2b: scale by 10^k. */
    if (k >= 0) {
        if (!proven_float_bigint_mul_pow5(&S, k) || !proven_float_bigint_shl_bits(&S, k)) return -1;
    } else {
        proven_i64 nk = -k;
        if (!proven_float_bigint_mul_pow5(&R, nk) || !proven_float_bigint_shl_bits(&R, nk)) return -1;
        if (!proven_float_bigint_mul_pow5(&mp, nk) || !proven_float_bigint_shl_bits(&mp, nk)) return -1;
        if (!proven_float_bigint_mul_pow5(&mm, nk) || !proven_float_bigint_shl_bits(&mm, nk)) return -1;
    }

    /* Step 3: fix up the estimate so the first generated digit is correct. */
    proven_float_bigint_copy(&tmp, &R);
    if (!proven_float_bigint_add(&tmp, &mp)) return -1;
    {
        int c = proven_float_bigint_cmp(&tmp, &S);
        bool need = even ? (c >= 0) : (c > 0);
        if (need) {
            if (!proven_float_bigint_mul_u64(&S, 10u)) return -1;
            k += 1;
        }
    }

    /* Step 4: generate digits. Leading digit sits at 10^(k-1). */
    *decimal_exp = k - 1;
    for (;;) {
        proven_u64 d;
        bool low;
        bool high;
        int cl;
        int ch;

        if (!proven_float_bigint_mul_u64(&R, 10u) ||
            !proven_float_bigint_mul_u64(&mp, 10u) ||
            !proven_float_bigint_mul_u64(&mm, 10u)) {
            return -1;
        }
        if (!proven_float_bigint_divmod(&R, &S, &q, &rem)) return -1;
        d = (q.len != 0u) ? q.limbs[0] : 0u;
        proven_float_bigint_copy(&R, &rem);

        cl = proven_float_bigint_cmp(&R, &mm);
        low = even ? (cl <= 0) : (cl < 0);
        proven_float_bigint_copy(&tmp, &R);
        if (!proven_float_bigint_add(&tmp, &mp)) return -1;
        ch = proven_float_bigint_cmp(&tmp, &S);
        high = even ? (ch >= 0) : (ch > 0);

        if (!low && !high) {
            if (pos + 1u >= out_cap) return -1;
            out[pos++] = (char)('0' + (int)d);
            continue;
        }

        /* Terminal digit. */
        {
            proven_u64 dd;
            if (low && !high) {
                dd = d;
            } else if (high && !low) {
                dd = d + 1u;
            } else {
                /* both boundaries: round to nearer via 2*R vs S, ties to even. */
                int cc;
                proven_float_bigint_copy(&tmp, &R);
                if (!proven_float_bigint_shl_bits(&tmp, 1)) return -1;
                cc = proven_float_bigint_cmp(&tmp, &S);
                if (cc > 0) {
                    dd = d + 1u;
                } else if (cc < 0) {
                    dd = d;
                } else {
                    dd = d + (d & 1u);
                }
            }
            if (dd < 10u) {
                if (pos + 1u >= out_cap) return -1;
                out[pos++] = (char)('0' + (int)dd);
            } else {
                /* Carry: this digit becomes 0 and 1 propagates into the prefix. */
                proven_i64 j = (proven_i64)pos - 1;
                int carry = 1;
                if (pos + 1u >= out_cap) return -1;
                out[pos++] = '0';
                while (j >= 0 && carry) {
                    int nv = (out[j] - '0') + carry;
                    out[j] = (char)('0' + (nv % 10));
                    carry = nv / 10;
                    --j;
                }
                if (carry) {
                    out[0] = '1';
                    pos = 1u;
                    *decimal_exp += 1;
                }
            }
            break;
        }
    }

    /* Trim trailing zeros (a terminal carry can introduce them). */
    while (pos > 1u && out[pos - 1u] == '0') {
        --pos;
    }
    out[pos] = '\0';
    return (int)pos;
}

/*
 * A canonical shortest result has a nonzero leading digit. Some boundary values
 * just below a power of ten leave the digit generators (both Grisu3 and Dragon4)
 * with a spurious leading zero and a decimal exponent one too high, e.g. digits
 * "0999..." at exponent k instead of "999..." at k-1. The trailing digits are
 * the correct shortest digits; only the leading position is off. Strip the
 * leading zero(s) and lower the decimal exponent to canonicalize, leaving the
 * value, round-trip property, and minimal length intact.
 */
static int proven_float_normalize_shortest_digits(char *out, int length, proven_i64 *decimal_exp) {
    if (length <= 0) {
        return length;
    }
    while (length > 1 && out[0] == '0') {
        int i;
        for (i = 1; i < length; ++i) {
            out[i - 1] = out[i];
        }
        length--;
        out[length] = '\0';
        *decimal_exp -= 1;
    }
    return length;
}

int proven_float_shortest_digits(double value, char *out, proven_size_t out_cap, proven_i64 *decimal_exp) {
    proven_u64 bits = proven_float_bits_f64(value) & 0x7fffffffffffffffull;
    proven_u64 f = 0;
    proven_i64 e = 0;
    int n;
    proven_float_f64_to_scaled(bits, &f, &e);
    /* boundary significand 2^52, minimum exponent -1074 for binary64. */
    {
        int g = proven_float_shortest_digits_grisu(f, e, 1ull << 52, -1074, out, out_cap, decimal_exp);
        n = (g >= 0) ? g
                     : proven_float_shortest_digits_core(f, e, 1ull << 52, -1074, out, out_cap, decimal_exp);
    }
    if (n < 0) {
        return n;
    }
    return proven_float_normalize_shortest_digits(out, n, decimal_exp);
}

int proven_float_shortest_digits_f32(float value, char *out, proven_size_t out_cap, proven_i64 *decimal_exp) {
    proven_u32 bits = proven_float_bits_f32(value) & 0x7fffffffu;
    proven_u32 frac = bits & 0x007fffffu;
    proven_u32 exp = (bits >> 23) & 0xffu;
    proven_u64 f;
    proven_i64 e;
    int n;

    if (exp == 0u) {
        f = frac;        /* subnormal (or zero) */
        e = -149;
    } else {
        f = (1u << 23) | frac;
        e = (proven_i64)exp - 150;
    }
    /* boundary significand 2^23, minimum exponent -149 for binary32. */
    {
        int g = proven_float_shortest_digits_grisu(f, e, 1u << 23, -149, out, out_cap, decimal_exp);
        n = (g >= 0) ? g
                     : proven_float_shortest_digits_core(f, e, 1u << 23, -149, out, out_cap, decimal_exp);
    }
    if (n < 0) {
        return n;
    }
    return proven_float_normalize_shortest_digits(out, n, decimal_exp);
}

static bool proven_float_decimal_build_number(const proven_u8 *input, proven_size_t len, proven_float_decimal_number_t *out) {
    proven_size_t cursor = 0;
    bool negative = false;
    bool seen_point = false;
    proven_i64 explicit_exp = 0;
    proven_size_t digits_total = 0;
    proven_size_t frac_digits = 0;
    proven_size_t first_nonzero = PROVEN_SIZE_MAX;
    proven_size_t last_nonzero = PROVEN_SIZE_MAX;
    proven_u64 mantissa_u64 = 0;
    bool mantissa_fits_u64 = true;
    proven_size_t significant_digits = 0;
    proven_size_t pending_zero_digits = 0;
    bool have_mantissa = false;

    if (cursor < len && (input[cursor] == (proven_u8)'+' || input[cursor] == (proven_u8)'-')) {
        negative = input[cursor] == (proven_u8)'-';
        ++cursor;
    }

    while (cursor < len && input[cursor] != (proven_u8)'e' && input[cursor] != (proven_u8)'E') {
        proven_u8 ch = input[cursor];
        if (ch == (proven_u8)'.') {
            seen_point = true;
        } else {
            proven_u32 digit = (proven_u32)(ch - (proven_u8)'0');

            if (digit != 0u) {
                if (first_nonzero == PROVEN_SIZE_MAX) {
                    first_nonzero = digits_total;
                }
                last_nonzero = digits_total;
                if (mantissa_fits_u64) {
                    if (!have_mantissa) {
                        mantissa_u64 = (proven_u64)digit;
                        have_mantissa = true;
                    } else {
                        while (pending_zero_digits > 0u) {
                            if (mantissa_u64 > 1844674407370955161ull) {
                                mantissa_fits_u64 = false;
                                break;
                            }
                            mantissa_u64 *= 10ull;
                            --pending_zero_digits;
                        }
                        if (mantissa_fits_u64) {
                            if (mantissa_u64 > 1844674407370955161ull ||
                                (mantissa_u64 == 1844674407370955161ull && (proven_u64)digit > 5ull)) {
                                mantissa_fits_u64 = false;
                            } else {
                                mantissa_u64 = mantissa_u64 * 10ull + (proven_u64)digit;
                            }
                        }
                    }
                }
                pending_zero_digits = 0u;
                ++significant_digits;
            } else if (first_nonzero != PROVEN_SIZE_MAX) {
                ++pending_zero_digits;
                ++significant_digits;
            }
            ++digits_total;
            if (seen_point) {
                ++frac_digits;
            }
        }
        ++cursor;
    }

    if (cursor < len && (input[cursor] == (proven_u8)'e' || input[cursor] == (proven_u8)'E')) {
        bool exp_negative = false;
        ++cursor;
        if (cursor < len && (input[cursor] == (proven_u8)'+' || input[cursor] == (proven_u8)'-')) {
            exp_negative = input[cursor] == (proven_u8)'-';
            ++cursor;
        }
        while (cursor < len && input[cursor] >= (proven_u8)'0' && input[cursor] <= (proven_u8)'9') {
            explicit_exp = proven_float_exp10_accumulate_clamped(
                explicit_exp,
                (proven_u8)(input[cursor] - (proven_u8)'0')
            );
            ++cursor;
        }
        if (exp_negative) {
            explicit_exp = -explicit_exp;
        }
    }

    out->negative = negative;
    out->is_zero = first_nonzero == PROVEN_SIZE_MAX;
    out->mantissa_fits_u64 = true;
    out->significand_sticky = false;
    out->mantissa_u64 = 0;
    out->significant_digits = 0;
    out->exp10 = 0;

    if (out->is_zero) {
        return true;
    }

    out->negative = negative;
    out->is_zero = false;
    out->mantissa_fits_u64 = mantissa_fits_u64;
    out->mantissa_u64 = mantissa_fits_u64 ? mantissa_u64 : 0ull;
    /*
     * Trailing zeros after the last nonzero digit are folded into exp10 and are
     * not stored in the significand or mantissa_u64. Exclude them here so
     * significant_digits matches the significand's true digit count; otherwise
     * the magnitude estimate (exp10 + significant_digits - 1) and the binary
     * exponent bounds derived from it are biased high, which can push the true
     * result outside the exact-search range.
     */
    out->significant_digits = significant_digits - pending_zero_digits;
    out->exp10 = explicit_exp - (proven_i64)frac_digits + (proven_i64)(digits_total - 1u - last_nonzero);
    out->digits_total = digits_total;
    out->frac_digits = frac_digits;
    out->first_nonzero = first_nonzero;
    out->last_nonzero = last_nonzero;

    return true;
}

/*
 * Builds the exact significand from the input digits, keeping at most
 * PROVEN_FLOAT_MAX_SIGNIFICAND_DIGITS significant digits. Significant digits
 * beyond the cap are dropped: *dropped_out counts them so the caller can shift
 * exp10 to preserve magnitude, and *sticky_out records whether any dropped digit
 * was nonzero (meaning the true value is strictly above the kept significand).
 */
static bool proven_float_decimal_build_significand(const proven_u8 *input, proven_size_t len,
                                                   const proven_float_decimal_number_t *decimal,
                                                   proven_float_bigint_t *out,
                                                   proven_size_t *dropped_out, bool *sticky_out) {
    proven_size_t cursor = 0;
    proven_size_t digit_index = 0;
    proven_size_t kept = 0;
    proven_size_t dropped = 0;
    bool sticky = false;

    if (!input || !decimal || !out || !dropped_out || !sticky_out) {
        return false;
    }

    out->len = 0u;
    *dropped_out = 0u;
    *sticky_out = false;
    if (decimal->is_zero) {
        return true;
    }

    if (input[cursor] == (proven_u8)'-' || input[cursor] == (proven_u8)'+') {
        ++cursor;
    }

    while (cursor < len && input[cursor] != (proven_u8)'e' && input[cursor] != (proven_u8)'E') {
            proven_u8 ch = input[cursor];
            if (ch >= (proven_u8)'0' && ch <= (proven_u8)'9') {
                proven_u32 digit = (proven_u32)(ch - (proven_u8)'0');
                if (digit_index >= decimal->first_nonzero && digit_index <= decimal->last_nonzero) {
                    if (kept < PROVEN_FLOAT_MAX_SIGNIFICAND_DIGITS) {
                        if (!proven_float_bigint_mul_small(out, 10u) || !proven_float_bigint_add_small(out, digit)) {
                            return false;
                        }
                        ++kept;
                    } else {
                        ++dropped;
                        if (digit != 0u) {
                            sticky = true;
                        }
                    }
                }
                ++digit_index;
        }
        ++cursor;
    }

    *dropped_out = dropped;
    *sticky_out = sticky;
    return true;
}

static int proven_float_compare_bigints_with_exp2(const proven_float_bigint_t *lhs_int, proven_i64 lhs_exp2,
                                                  const proven_float_bigint_t *rhs_int, proven_i64 rhs_exp2) {
    if (!lhs_int || !rhs_int) {
        return -1;
    }

    if (lhs_exp2 > rhs_exp2) {
        return proven_float_bigint_cmp_shift_left(lhs_int, (proven_size_t)(lhs_exp2 - rhs_exp2), rhs_int);
    }

    if (rhs_exp2 > lhs_exp2) {
        return -proven_float_bigint_cmp_shift_left(rhs_int, (proven_size_t)(rhs_exp2 - lhs_exp2), lhs_int);
    }

    return proven_float_bigint_cmp(lhs_int, rhs_int);
}

static proven_u128_parts_t proven_float_u128_shift_u64(proven_u64 value, unsigned shift) {
    if (shift == 0u) {
        return (proven_u128_parts_t){ .lo = value, .hi = 0u };
    }
    if (shift < 64u) {
        return (proven_u128_parts_t){
            .lo = value << shift,
            .hi = value >> (64u - shift),
        };
    }
    if (shift < 128u) {
        return (proven_u128_parts_t){
            .lo = 0u,
            .hi = value << (shift - 64u),
        };
    }
    return (proven_u128_parts_t){ .lo = 0u, .hi = 0u };
}

static proven_u128_parts_t proven_float_u128_add(proven_u128_parts_t lhs, proven_u128_parts_t rhs) {
    proven_u64 lo = lhs.lo + rhs.lo;
    proven_u64 carry = lo < lhs.lo ? 1ull : 0ull;
    return (proven_u128_parts_t){
        .lo = lo,
        .hi = lhs.hi + rhs.hi + carry,
    };
}

static bool proven_float_bigint_from_adjacent_midpoint(
    proven_u64 lower_mantissa,
    proven_i64 lower_exp2,
    proven_u64 upper_mantissa,
    proven_i64 upper_exp2,
    proven_float_bigint_t *midpoint_num,
    proven_i64 *mid_exp2
) {
    proven_i64 min_exp2 = lower_exp2 < upper_exp2 ? lower_exp2 : upper_exp2;
    unsigned lower_shift = (unsigned)(lower_exp2 - min_exp2);
    unsigned upper_shift = (unsigned)(upper_exp2 - min_exp2);
    proven_u128_parts_t sum;

    if (!midpoint_num || !mid_exp2) {
        return false;
    }

    if (lower_shift == 0u && upper_shift == 0u) {
        sum.lo = lower_mantissa + upper_mantissa;
        sum.hi = (sum.lo < lower_mantissa) ? 1u : 0u;
    } else {
        sum = proven_float_u128_add(
            proven_float_u128_shift_u64(lower_mantissa, lower_shift),
            proven_float_u128_shift_u64(upper_mantissa, upper_shift)
        );
    }

    proven_float_bigint_set_u128_parts(midpoint_num, sum);
    *mid_exp2 = min_exp2 - 1;
    return true;
}

typedef struct {
    proven_i64 exp10;
    bool pow5_factor_ready;
    proven_float_bigint_t pow5_factor;
    proven_float_bigint_t scaled_significand;
    bool scaled_significand_ready;
} proven_float_exact_compare_state_t;

typedef struct {
    proven_i64 exp10;
    bool pow5_factor_ready;
    proven_float_bigint_t pow5_factor;
} proven_float_eisel_validate_state_t;

static bool proven_float_prepare_exact_compare_state(const proven_float_decimal_number_t *decimal,
                                                     proven_float_exact_compare_state_t *state) {
    if (!decimal || !state) {
        return false;
    }

    state->exp10 = decimal->exp10;
    state->pow5_factor_ready = false;
    state->pow5_factor.limbs[0] = 1u;
    state->pow5_factor.len = 1u;
    state->scaled_significand_ready = false;

    if (decimal->exp10 == 0) {
        state->pow5_factor_ready = true;
    } else if (proven_float_bigint_build_pow5_cached(decimal->exp10 < 0 ? -decimal->exp10 : decimal->exp10, &state->pow5_factor)) {
        state->pow5_factor_ready = true;
    }
    if (decimal->exp10 > 0) {
        proven_float_bigint_copy(&state->scaled_significand, &decimal->significand);
        if (state->pow5_factor_ready && proven_float_bigint_mul_factor(&state->scaled_significand, &state->pow5_factor)) {
            state->scaled_significand_ready = true;
        }
    }

    return true;
}

[[maybe_unused]]
static bool proven_float_prepare_eisel_validate_state(const proven_float_decimal_number_t *decimal,
                                                      proven_float_eisel_validate_state_t *state) {
    if (!decimal || !state) {
        return false;
    }

    state->exp10 = decimal->exp10;
    state->pow5_factor_ready = false;
    state->pow5_factor.limbs[0] = 1u;
    state->pow5_factor.len = 1u;

    if (decimal->exp10 == 0) {
        state->pow5_factor_ready = true;
        return true;
    }

    if (proven_float_bigint_build_pow5_cached(decimal->exp10 < 0 ? -decimal->exp10 : decimal->exp10, &state->pow5_factor)) {
        state->pow5_factor_ready = true;
    }

    return true;
}

static int proven_float_compare_mantissa_to_scaled_legacy(const proven_float_decimal_number_t *decimal,
                                                          const proven_float_bigint_t *rhs_int, proven_i64 rhs_exp2) {
    proven_float_bigint_t lhs_int;
    proven_float_bigint_t rhs_work;
    proven_float_bigint_t pow5_factor;
    proven_i64 lhs_exp2 = 0;
    bool rhs_work_ready = false;

    if (!decimal || !rhs_int) {
        return -1;
    }

    proven_float_bigint_set_u64(&lhs_int, decimal->mantissa_u64);

    if (decimal->exp10 >= 0) {
        if (!proven_float_bigint_build_pow5_cached(decimal->exp10, &pow5_factor)) {
            return 1;
        }
        if (!proven_float_bigint_mul_factor(&lhs_int, &pow5_factor)) {
            return 1;
        }
        lhs_exp2 = decimal->exp10;
    } else {
        proven_i64 neg_exp10 = -decimal->exp10;
        if (!proven_float_bigint_build_pow5_cached(neg_exp10, &pow5_factor)) {
            return -1;
        }
        rhs_work_ready = true;
        proven_float_bigint_copy(&rhs_work, rhs_int);
        if (!proven_float_bigint_mul_factor(&rhs_work, &pow5_factor)) {
            return -1;
        }
        rhs_exp2 += neg_exp10;
    }

    if (lhs_exp2 > rhs_exp2) {
        if (!proven_float_bigint_shl_bits(&lhs_int, lhs_exp2 - rhs_exp2)) {
            return 1;
        }
    } else if (rhs_exp2 > lhs_exp2) {
        if (!rhs_work_ready) {
            proven_float_bigint_copy(&rhs_work, rhs_int);
            rhs_work_ready = true;
        }
        if (!proven_float_bigint_shl_bits(&rhs_work, rhs_exp2 - lhs_exp2)) {
            return -1;
        }
    }

    if (rhs_work_ready) {
        return proven_float_bigint_cmp(&lhs_int, &rhs_work);
    }
    return proven_float_bigint_cmp(&lhs_int, rhs_int);
}

static int proven_float_compare_mantissa_to_scaled_with_state(const proven_float_decimal_number_t *decimal,
                                                              const proven_float_eisel_validate_state_t *state,
                                                              const proven_float_bigint_t *rhs_int, proven_i64 rhs_exp2) {
    if (!decimal || !state || !rhs_int) {
        return -1;
    }

    if (decimal->exp10 > 0) {
        proven_float_bigint_t lhs_int;
        proven_float_bigint_set_u64(&lhs_int, decimal->mantissa_u64);
        if (state->pow5_factor_ready) {
            if (!proven_float_bigint_mul_factor(&lhs_int, &state->pow5_factor)) {
                return proven_float_compare_mantissa_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
            }
            return proven_float_compare_bigints_with_exp2(&lhs_int, decimal->exp10, rhs_int, rhs_exp2);
        }
        return proven_float_compare_mantissa_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
    }

    if (decimal->exp10 < 0) {
        if (!state->pow5_factor_ready) {
            return proven_float_compare_mantissa_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
        }
        proven_float_bigint_t lhs_int;
        proven_float_bigint_set_u64(&lhs_int, decimal->mantissa_u64);
        proven_float_bigint_t rhs_work;
        if (rhs_int->len == 1u) {
            if (!proven_float_bigint_mul_u64_factor(&rhs_work, rhs_int->limbs[0], &state->pow5_factor)) {
                return proven_float_compare_mantissa_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
            }
        } else {
            proven_float_bigint_copy(&rhs_work, rhs_int);
            if (!proven_float_bigint_mul_factor(&rhs_work, &state->pow5_factor)) {
                return proven_float_compare_mantissa_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
            }
        }
        return proven_float_compare_bigints_with_exp2(&lhs_int, 0, &rhs_work, rhs_exp2 - decimal->exp10);
    }

    {
        proven_float_bigint_t lhs_int;
        proven_float_bigint_set_u64(&lhs_int, decimal->mantissa_u64);
        return proven_float_compare_bigints_with_exp2(&lhs_int, 0, rhs_int, rhs_exp2);
    }
}

static int proven_float_compare_mantissa_to_bits(const proven_float_decimal_number_t *decimal,
                                                 const proven_float_eisel_validate_state_t *state,
                                                 proven_u64 bits) {
    proven_u64 mantissa = 0;
    proven_i64 exp2 = 0;
    proven_float_bigint_t rhs_int;

    proven_float_f64_to_scaled(bits, &mantissa, &exp2);
    proven_float_bigint_set_u64(&rhs_int, mantissa);
    return proven_float_compare_mantissa_to_scaled_with_state(decimal, state, &rhs_int, exp2);
}

static int proven_float_compare_mantissa_to_midpoint(const proven_float_decimal_number_t *decimal,
                                                     const proven_float_eisel_validate_state_t *state,
                                                     proven_u64 lower_bits, proven_u64 upper_bits) {
    proven_u64 lower_mantissa = 0;
    proven_u64 upper_mantissa = 0;
    proven_i64 lower_exp2 = 0;
    proven_i64 upper_exp2 = 0;
    proven_float_bigint_t midpoint_num;

    proven_float_f64_to_scaled(lower_bits, &lower_mantissa, &lower_exp2);
    proven_float_f64_to_scaled(upper_bits, &upper_mantissa, &upper_exp2);
    if (!proven_float_bigint_from_adjacent_midpoint(lower_mantissa, lower_exp2, upper_mantissa, upper_exp2, &midpoint_num, &lower_exp2)) {
        return -1;
    }

    return proven_float_compare_mantissa_to_scaled_with_state(decimal, state, &midpoint_num, lower_exp2);
}

static int proven_float_compare_decimal_to_scaled_legacy(const proven_float_decimal_number_t *decimal,
                                                         const proven_float_bigint_t *rhs_int, proven_i64 rhs_exp2) {
    proven_float_bigint_t lhs_int;
    proven_float_bigint_t rhs_work;
    proven_i64 lhs_exp2 = 0;
    bool rhs_work_ready = false;

    proven_float_bigint_copy(&lhs_int, &decimal->significand);

    if (decimal->exp10 >= 0) {
        if (!proven_float_bigint_mul_pow5(&lhs_int, decimal->exp10)) {
            return 1;
        }
        lhs_exp2 = decimal->exp10;
    } else {
        proven_i64 neg_exp10 = -decimal->exp10;
        proven_float_bigint_copy(&rhs_work, rhs_int);
        rhs_work_ready = true;
        if (!proven_float_bigint_mul_pow5(&rhs_work, neg_exp10)) {
            return -1;
        }
        rhs_exp2 += neg_exp10;
    }

    if (lhs_exp2 > rhs_exp2) {
        if (!proven_float_bigint_shl_bits(&lhs_int, lhs_exp2 - rhs_exp2)) {
            return 1;
        }
    } else if (rhs_exp2 > lhs_exp2) {
        if (!rhs_work_ready) {
            proven_float_bigint_copy(&rhs_work, rhs_int);
            rhs_work_ready = true;
        }
        if (!proven_float_bigint_shl_bits(&rhs_work, rhs_exp2 - lhs_exp2)) {
            return -1;
        }
    }

    if (rhs_work_ready) {
        return proven_float_bigint_cmp(&lhs_int, &rhs_work);
    }
    return proven_float_bigint_cmp(&lhs_int, rhs_int);
}

static int proven_float_compare_decimal_to_scaled_raw(const proven_float_decimal_number_t *decimal,
                                                      const proven_float_exact_compare_state_t *state,
                                                      const proven_float_bigint_t *rhs_int, proven_i64 rhs_exp2) {
    if (!decimal || !state || !rhs_int) {
        return -1;
    }

    if (decimal->exp10 > 0) {
        if (state->pow5_factor_ready && state->scaled_significand_ready) {
            return proven_float_compare_bigints_with_exp2(&state->scaled_significand, decimal->exp10, rhs_int, rhs_exp2);
        }
        return proven_float_compare_decimal_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
    }

    if (decimal->exp10 < 0) {
        if (!state->pow5_factor_ready) {
            return proven_float_compare_decimal_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
        }
        proven_float_bigint_t rhs_work;
        if (rhs_int->len == 1u) {
            if (!proven_float_bigint_mul_u64_factor(&rhs_work, rhs_int->limbs[0], &state->pow5_factor)) {
                return proven_float_compare_decimal_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
            }
        } else {
            if (!proven_float_bigint_copy_mul_factor(&rhs_work, rhs_int, &state->pow5_factor)) {
                return proven_float_compare_decimal_to_scaled_legacy(decimal, rhs_int, rhs_exp2);
            }
        }
        return proven_float_compare_bigints_with_exp2(&decimal->significand, 0, &rhs_work, rhs_exp2 - decimal->exp10);
    }

    return proven_float_compare_bigints_with_exp2(&decimal->significand, 0, rhs_int, rhs_exp2);
}

/*
 * Sticky-aware comparison of the decimal value against a scaled candidate.
 * The stored significand may be a truncation of a longer input; when it compares
 * exactly equal to the candidate but dropped digits were nonzero, the true value
 * is strictly greater, so the tie is broken upward. With the significand cap at
 * least as large as the longest binary64 rounding boundary, an exact-equal raw
 * result implies the candidate has no digits past the kept prefix, so this tie
 * break is exact.
 */
static int proven_float_compare_decimal_to_scaled(const proven_float_decimal_number_t *decimal,
                                                  const proven_float_exact_compare_state_t *state,
                                                  const proven_float_bigint_t *rhs_int, proven_i64 rhs_exp2) {
    int raw = proven_float_compare_decimal_to_scaled_raw(decimal, state, rhs_int, rhs_exp2);
    if (raw == 0 && decimal && decimal->significand_sticky) {
        return 1;
    }
    return raw;
}

static int proven_float_compare_decimal_to_bits(const proven_float_decimal_number_t *decimal,
                                                const proven_float_exact_compare_state_t *state,
                                                proven_u64 bits) {
    proven_u64 mantissa = 0;
    proven_i64 exp2 = 0;
    proven_float_bigint_t rhs_int;

    proven_float_f64_to_scaled(bits, &mantissa, &exp2);
    proven_float_bigint_set_u64(&rhs_int, mantissa);
    return proven_float_compare_decimal_to_scaled(decimal, state, &rhs_int, exp2);
}

static int proven_float_compare_decimal_to_midpoint(const proven_float_decimal_number_t *decimal,
                                                    const proven_float_exact_compare_state_t *state,
                                                    proven_u64 lower_bits, proven_u64 upper_bits) {
    proven_u64 lower_mantissa = 0;
    proven_u64 upper_mantissa = 0;
    proven_i64 lower_exp2 = 0;
    proven_i64 upper_exp2 = 0;
    proven_float_bigint_t midpoint_num;

    proven_float_f64_to_scaled(lower_bits, &lower_mantissa, &lower_exp2);
    proven_float_f64_to_scaled(upper_bits, &upper_mantissa, &upper_exp2);
    if (!proven_float_bigint_from_adjacent_midpoint(lower_mantissa, lower_exp2, upper_mantissa, upper_exp2, &midpoint_num, &lower_exp2)) {
        return -1;
    }

    return proven_float_compare_decimal_to_scaled(decimal, state, &midpoint_num, lower_exp2);
}

[[maybe_unused]]
static proven_float_fast_path_result_t proven_float_validate_eisel_lemire_candidate(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_u64 candidate_bits,
    proven_u64 *bits_out
) {
    const proven_u64 max_finite_bits = 0x7fefffffffffffffull;
    int cmp = 0;
    int cmp_mid = 0;

    if (decimal == 0 || bits_out == 0 || state == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    cmp = proven_float_compare_mantissa_to_bits(decimal, state, candidate_bits);
    if (cmp == 0) {
        *bits_out = candidate_bits;
        return PROVEN_FLOAT_FAST_PATH_SUCCESS;
    }

    if (cmp < 0) {
        if (candidate_bits == 0u) {
            return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
        }
        cmp_mid = proven_float_compare_mantissa_to_midpoint(decimal, state, candidate_bits - 1u, candidate_bits);
        if (cmp_mid > 0 || (cmp_mid == 0 && (candidate_bits & 1ull) == 0u)) {
            *bits_out = candidate_bits;
            return PROVEN_FLOAT_FAST_PATH_SUCCESS;
        }
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }

    if (candidate_bits == max_finite_bits) {
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }

    cmp_mid = proven_float_compare_mantissa_to_midpoint(decimal, state, candidate_bits, candidate_bits + 1u);
    if (cmp_mid < 0 || (cmp_mid == 0 && (candidate_bits & 1ull) == 0u)) {
        *bits_out = candidate_bits;
        return PROVEN_FLOAT_FAST_PATH_SUCCESS;
    }
    return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
}

/* Defined unconditionally: it needs no 128-bit integers and is used by both the
 * Eisel-Lemire fast path and the estimate path (which is always compiled). */
static proven_float_fast_path_result_t proven_float_pack_binary64_candidate(
    proven_u64 sig,
    proven_i64 unbiased_exp,
    proven_u64 *bits_out
) {
    proven_u64 frac = 0;
    proven_i64 shift = 0;

    if (bits_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }
    if (sig < (1ull << 52) || sig >= (1ull << 53)) {
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }
    if (unbiased_exp > 1023) {
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }

    if (unbiased_exp >= -1022) {
        *bits_out = ((proven_u64)(unbiased_exp + 1023) << 52) | (sig & 0x000fffffffffffffull);
        return PROVEN_FLOAT_FAST_PATH_SUCCESS;
    }

    shift = -1022 - unbiased_exp;
    if (shift >= 64) {
        *bits_out = 0u;
        return PROVEN_FLOAT_FAST_PATH_SUCCESS;
    }

    frac = sig >> (unsigned)shift;
    if (shift > 0) {
        proven_u64 mask = (1ull << (unsigned)shift) - 1ull;
        proven_u64 rem = sig & mask;
        proven_u64 halfway = 1ull << (unsigned)(shift - 1);
        if (rem > halfway || (rem == halfway && (frac & 1ull) != 0u)) {
            ++frac;
        }
    }

    if (frac >= (1ull << 52)) {
        *bits_out = 0x0010000000000000ull;
        return PROVEN_FLOAT_FAST_PATH_SUCCESS;
    }

    *bits_out = frac;
    return PROVEN_FLOAT_FAST_PATH_SUCCESS;
}

[[maybe_unused]]
static proven_float_fast_path_result_t proven_float_finalize_eisel_lemire_candidate_bits(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_u64 candidate_bits,
    proven_u64 *bits_out
) {
    proven_u64 exp_bits = (candidate_bits >> 52) & 0x7ffull;

    if (decimal == 0 || bits_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }
    if (candidate_bits == 0u || exp_bits == 0x7ffull) {
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }

    return proven_float_validate_eisel_lemire_candidate(decimal, state, candidate_bits, bits_out);
}

[[maybe_unused]]
static proven_float_fast_path_result_t proven_float_finalize_eisel_lemire_significand(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_u64 sig,
    proven_i64 unbiased_exp,
    proven_u64 *bits_out
) {
    proven_u64 candidate_bits = 0;

    if (proven_float_pack_binary64_candidate(sig, unbiased_exp, &candidate_bits) != PROVEN_FLOAT_FAST_PATH_SUCCESS) {
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }

    return proven_float_finalize_eisel_lemire_candidate_bits(decimal, state, candidate_bits, bits_out);
}

static proven_err_t proven_float_try_clinger(const proven_float_decimal_number_t *decimal, double *out) {
    static const double pow10_exact[] = {
        1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
        1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
    };

    if (!decimal->mantissa_fits_u64 || decimal->mantissa_u64 > 9007199254740991ull) {
        return PROVEN_ERR_UNSUPPORTED;
    }
    if (decimal->exp10 < -22 || decimal->exp10 > 22) {
        return PROVEN_ERR_UNSUPPORTED;
    }

    if (decimal->exp10 >= 0) {
        *out = (double)decimal->mantissa_u64 * pow10_exact[(proven_size_t)decimal->exp10];
    } else {
        *out = (double)decimal->mantissa_u64 / pow10_exact[(proven_size_t)(-decimal->exp10)];
    }
    if (decimal->negative) {
        *out = -*out;
    }
    return PROVEN_OK;
}

#if defined(__SIZEOF_INT128__)
typedef struct proven_u256_parts_t {
    proven_u64 limb0;
    proven_u64 limb1;
    proven_u64 limb2;
    proven_u64 limb3;
} proven_u256_parts_t;

static proven_u256_parts_t proven_float_mul_u64_u128_to_u256(proven_u64 lhs, proven_u128_parts_t rhs) {
    proven_u128_parts_t lo_prod = proven_float_mul_u64_u64_to_u128(lhs, rhs.lo);
    proven_u128_parts_t hi_prod = proven_float_mul_u64_u64_to_u128(lhs, rhs.hi);
    proven_u64 limb1 = lo_prod.hi + hi_prod.lo;
    proven_u64 carry = limb1 < lo_prod.hi ? 1ull : 0ull;
    return (proven_u256_parts_t){
        .limb0 = lo_prod.lo,
        .limb1 = limb1,
        .limb2 = hi_prod.hi + carry,
        .limb3 = 0u,
    };
}

static int proven_float_u256_bit_length(proven_u256_parts_t value) {
    if (value.limb3 != 0u) {
        return 256 - proven_float_clz_u64(value.limb3);
    }
    if (value.limb2 != 0u) {
        return 192 - proven_float_clz_u64(value.limb2);
    }
    if (value.limb1 != 0u) {
        return 128 - proven_float_clz_u64(value.limb1);
    }
    if (value.limb0 != 0u) {
        return 64 - proven_float_clz_u64(value.limb0);
    }
    return 0;
}

static proven_u64 proven_float_u256_shr_to_u64(proven_u256_parts_t value, unsigned shift, bool *sticky) {
    const proven_u64 *limbs = &value.limb0;
    proven_u64 result = 0;
    bool extra = false;
    unsigned limb = shift / 64u;
    unsigned bits = shift % 64u;

    if (shift >= 256u) {
        extra = value.limb0 != 0u || value.limb1 != 0u || value.limb2 != 0u || value.limb3 != 0u;
        result = 0u;
    } else {
        result = limbs[limb] >> bits;
        if (bits != 0u && limb + 1u < 4u) {
            result |= limbs[limb + 1u] << (64u - bits);
        }
        for (unsigned i = 0; i < limb; ++i) {
            extra = extra || limbs[i] != 0u;
        }
        if (bits != 0u) {
            proven_u64 mask = (1ull << bits) - 1ull;
            extra = extra || (limbs[limb] & mask) != 0u;
        }
    }

    if (sticky) {
        *sticky = extra;
    }
    return result;
}

static proven_float_fast_path_result_t proven_float_round_u256_to_significand(
    proven_u256_parts_t integer,
    proven_i64 exp2,
    proven_u64 *sig_out,
    proven_i64 *unbiased_exp_out
) {
    int bit_length = proven_float_u256_bit_length(integer);
    int shift = bit_length - 53;
    proven_u64 significand = 0;
    bool sticky = false;
    proven_i64 unbiased_exp = exp2 + (proven_i64)bit_length - 1;

    if (bit_length == 0 || sig_out == 0 || unbiased_exp_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    if (shift <= 0) {
        significand = integer.limb0 << (unsigned)(-shift);
    } else {
        proven_u64 truncated = proven_float_u256_shr_to_u64(integer, (unsigned)shift, NULL);
        bool round_bit = false;
        if ((unsigned)(shift - 1) < 256u) {
            round_bit = (proven_float_u256_shr_to_u64(integer, (unsigned)(shift - 1), NULL) & 1ull) != 0u;
        }
        if (shift > 1) {
            (void)proven_float_u256_shr_to_u64(integer, (unsigned)(shift - 1), &sticky);
        }
        significand = truncated;
        if (round_bit && (sticky || (significand & 1ull) != 0u)) {
            ++significand;
            if (significand == (1ull << 53)) {
                significand >>= 1;
                ++unbiased_exp;
            }
        }
    }

    *sig_out = significand;
    *unbiased_exp_out = unbiased_exp;
    return PROVEN_FLOAT_FAST_PATH_SUCCESS;
}

static proven_float_fast_path_result_t proven_float_try_eisel_lemire_product_u256(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_u256_parts_t product,
    proven_i64 exp2,
    proven_u64 *bits_out
) {
    proven_u64 sig = 0;
    proven_i64 unbiased_exp = 0;

    if (proven_float_round_u256_to_significand(product, exp2, &sig, &unbiased_exp) != PROVEN_FLOAT_FAST_PATH_SUCCESS) {
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }

    return proven_float_finalize_eisel_lemire_significand(decimal, state, sig, unbiased_exp, bits_out);
}

static proven_float_fast_path_result_t proven_float_try_eisel_lemire_cached_power_product(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_u64 integer,
    proven_u128_parts_t cached_power,
    proven_i64 exp2,
    proven_u64 *bits_out
) {
    proven_u256_parts_t product;

    if (decimal == 0 || integer == 0u || bits_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    product = proven_float_mul_u64_u128_to_u256(integer, cached_power);
    return proven_float_try_eisel_lemire_product_u256(decimal, state, product, exp2, bits_out);
}

static bool proven_float_build_cached_power_product_plan(
    proven_i64 exp10,
    proven_float_eisel_lemire_candidate_plan_t *plan
) {
    if (plan == 0) {
        return false;
    }
    if (exp10 >= 0) {
        proven_size_t q = (proven_size_t)exp10;
        if (q <= PROVEN_FLOAT_CACHED_POW5_U128_MAX_Q) {
            proven_float_cached_pow5_u128_entry_t entry = proven_float_cached_pow5_u128[q];
            *plan = (proven_float_eisel_lemire_candidate_plan_t){
                .mode = PROVEN_FLOAT_EISEL_LEMIRE_CANDIDATE_PRODUCT,
                .wide = { .lo = entry.lo, .hi = entry.hi },
                .exp2 = (proven_i64)q,
            };
            return true;
        } else if (q <= PROVEN_FLOAT_CACHED_POW5_SCALED_U128_MAX_Q) {
            proven_float_cached_pow5_scaled_u128_entry_t entry = proven_float_cached_pow5_scaled_u128[q];
            *plan = (proven_float_eisel_lemire_candidate_plan_t){
                .mode = PROVEN_FLOAT_EISEL_LEMIRE_CANDIDATE_PRODUCT,
                .wide = { .lo = entry.lo, .hi = entry.hi },
                .exp2 = (proven_i64)q + (proven_i64)entry.shift,
            };
            return true;
        }
        return false;
    }

    {
        proven_size_t q = (proven_size_t)(-exp10);
        if (q > PROVEN_FLOAT_CACHED_POW5_INV_U128_MAX_Q) {
            return false;
        }
        proven_float_cached_pow5_inv_u128_entry_t entry = proven_float_cached_pow5_inv_u128[q];
        *plan = (proven_float_eisel_lemire_candidate_plan_t){
            .mode = PROVEN_FLOAT_EISEL_LEMIRE_CANDIDATE_PRODUCT,
            .wide = { .lo = entry.lo, .hi = entry.hi },
            .exp2 = -((proven_i64)entry.shift) - (proven_i64)q,
        };
        return true;
    }
}

static proven_float_fast_path_result_t proven_float_execute_eisel_lemire_plan(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    const proven_float_eisel_lemire_candidate_plan_t *plan,
    proven_u64 *bits_out,
    proven_float_decimal_stats_t *stats
) {
    if (decimal == 0 || plan == 0 || bits_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    switch (plan->mode) {
        case PROVEN_FLOAT_EISEL_LEMIRE_CANDIDATE_PRODUCT: {
            proven_float_fast_path_result_t result = proven_float_try_eisel_lemire_cached_power_product(
                decimal,
                state,
                decimal->mantissa_u64,
                plan->wide,
                plan->exp2,
                bits_out
            );
            if (result == PROVEN_FLOAT_FAST_PATH_SUCCESS && stats != 0) {
                ++stats->eisel_lemire_product_plan_hits;
            }
            return result;
        }
    }

    return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
}

static proven_float_fast_path_result_t proven_float_try_eisel_lemire_pow5_product_q(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_size_t q,
    proven_u64 *bits_out,
    proven_float_decimal_stats_t *stats
) {
    proven_float_eisel_lemire_candidate_plan_t plan;

    if (decimal == 0 || bits_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }
    if (!proven_float_build_cached_power_product_plan((proven_i64)q, &plan)) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    return proven_float_execute_eisel_lemire_plan(decimal, state, &plan, bits_out, stats);
}

static proven_float_fast_path_result_t proven_float_try_eisel_lemire_negative_q(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_size_t q,
    proven_u64 *bits_out,
    proven_float_decimal_stats_t *stats
) {
    proven_float_eisel_lemire_candidate_plan_t plan;

    if (decimal == 0 || bits_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    if (proven_float_build_cached_power_product_plan(-(proven_i64)q, &plan)) {
        return proven_float_execute_eisel_lemire_plan(decimal, state, &plan, bits_out, stats);
    }
    return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
}

#else  /* !defined(__SIZEOF_INT128__) */

/*
 * Without 128-bit integers the Eisel-Lemire fast path is unavailable. These
 * stubs let `proven_float_try_eisel_lemire_with_state` (compiled unconditionally)
 * resolve its calls and report "unsupported", so parsing falls through to the
 * scalar exact path. The big-integer multiply has its own scalar fallback.
 */
static proven_float_fast_path_result_t proven_float_try_eisel_lemire_pow5_product_q(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_size_t q,
    proven_u64 *bits_out,
    proven_float_decimal_stats_t *stats
) {
    (void)decimal; (void)state; (void)q; (void)bits_out; (void)stats;
    return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
}

static proven_float_fast_path_result_t proven_float_try_eisel_lemire_negative_q(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_size_t q,
    proven_u64 *bits_out,
    proven_float_decimal_stats_t *stats
) {
    (void)decimal; (void)state; (void)q; (void)bits_out; (void)stats;
    return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
}

#endif

static proven_float_fast_path_result_t proven_float_try_eisel_lemire_with_state(
    const proven_float_decimal_number_t *decimal,
    const proven_float_eisel_validate_state_t *state,
    proven_u64 *bits_out,
    proven_float_decimal_stats_t *stats
) {
    /*
     * RFC-0001 target: this layer grows into a full Eisel-Lemire candidate path.
     * Current subset:
     * - requires a 64-bit significand
     * - routes both positive and negative decimal exponents through one
     *   signed cached-power product plan builder
     * - returns UNCERTAIN instead of guessing outside the checked subset
     */
    if (!decimal->mantissa_fits_u64 || decimal->significant_digits > 19u) {
        return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
    }
    if (decimal->exp10 > (proven_i64)PROVEN_FLOAT_CACHED_POW5_SCALED_U128_MAX_Q ||
        decimal->exp10 < -(proven_i64)PROVEN_FLOAT_CACHED_POW5_INV_U128_MAX_Q) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }
    if (bits_out == 0) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    if (decimal->exp10 > 0) {
        return proven_float_try_eisel_lemire_pow5_product_q(
            decimal, state, (proven_size_t)decimal->exp10, bits_out, stats);
    }

    if (decimal->exp10 < 0) {
        proven_size_t q = (proven_size_t)(-decimal->exp10);
        return proven_float_try_eisel_lemire_negative_q(decimal, state, q, bits_out, stats);
    }

    if (decimal->exp10 == 0) {
        return proven_float_try_eisel_lemire_pow5_product_q(decimal, state, 0u, bits_out, stats);
    }

    return PROVEN_FLOAT_FAST_PATH_UNCERTAIN;
}

static proven_float_fast_path_result_t proven_float_try_eisel_lemire(const proven_float_decimal_number_t *decimal,
                                                                     proven_u64 *bits_out,
                                                                     proven_float_decimal_stats_t *stats) {
    proven_float_eisel_validate_state_t state;

    if (!proven_float_prepare_eisel_validate_state(decimal, &state)) {
        return PROVEN_FLOAT_FAST_PATH_UNSUPPORTED;
    }

    return proven_float_try_eisel_lemire_with_state(decimal, &state, bits_out, stats);
}

/*
 * Pull a normal positive double back into [0.5, 1) and fold the binary exponent
 * it carried into *exp_acc. This is a freestanding-safe frexp specialised for
 * the always-normal, always-positive intermediates of the estimate path; it
 * works purely on the bit pattern so the PAL math boundary is preserved.
 */
static double proven_float_frexp_normal(double x, proven_i64 *exp_acc) {
    proven_u64 bits = proven_float_bits_f64(x);
    proven_u64 exp_field = (bits >> 52) & 0x7ffull;

    *exp_acc += (proven_i64)exp_field - 1022;
    bits = (bits & ~(0x7ffull << 52)) | (1022ull << 52);
    return proven_float_from_bits(bits);
}

/*
 * Best-effort estimate of the magnitude bits for a decimal that has reached the
 * exact fallback. The value is significand x 10^exp10; this reconstructs it as a
 * normalised double mantissa times a power of two, multiplies in 5^exp10 with
 * exact chunked powers and bit-level renormalisation (so no intermediate over-
 * or underflows), then packs the result with the shared candidate packer.
 *
 * Returns false when no usable estimate is available so the caller falls back to
 * the full exponent-bracket search. The estimate is only a seed: the exact
 * binary search verifies a window around it before trusting it, so a few ULP of
 * error here only affects speed, never the rounded result.
 */
static bool proven_float_estimate_value_bits(const proven_float_decimal_number_t *decimal, proven_u64 *bits_out) {
    /* 5^0 .. 5^22, all exactly representable in binary64 (5^22 < 2^53). */
    static const double pow5_exact[] = {
        1.0, 5.0, 25.0, 125.0, 625.0, 3125.0, 15625.0, 78125.0, 390625.0,
        1953125.0, 9765625.0, 48828125.0, 244140625.0, 1220703125.0,
        6103515625.0, 30517578125.0, 152587890625.0, 762939453125.0,
        3814697265625.0, 19073486328125.0, 95367431640625.0,
        476837158203125.0, 2384185791015625.0,
    };
    const double pow5_chunk = 2384185791015625.0; /* 5^22 */
    const proven_float_bigint_t *sig = &decimal->significand;
    proven_i64 acc2;
    proven_i64 nbits;
    proven_u64 top;
    proven_u64 hi_limb;
    proven_u64 mant_sig;
    double md;
    proven_size_t n;
    int lz;

    if (bits_out == 0 || sig->len == 0u) {
        return false;
    }

    /* Reconstruct the significand as top x 2^(nbits-64), top in [2^63, 2^64). */
    n = sig->len;
    hi_limb = sig->limbs[n - 1u];
    lz = proven_float_clz_u64(hi_limb);
    nbits = (proven_i64)(n - 1u) * 64 + (64 - lz);
    if (lz == 0) {
        top = hi_limb;
    } else {
        proven_u64 lo_limb = (n >= 2u) ? sig->limbs[n - 2u] : 0u;
        top = (hi_limb << lz) | (lo_limb >> (64 - lz));
    }

    acc2 = nbits - 64;
    md = proven_float_frexp_normal((double)top, &acc2);

    /* Multiply in 5^exp10 using exact chunks, renormalising md each step. */
    if (decimal->exp10 >= 0) {
        proven_i64 e = decimal->exp10;
        while (e >= 22) {
            md = proven_float_frexp_normal(md * pow5_chunk, &acc2);
            e -= 22;
        }
        if (e > 0) {
            md = proven_float_frexp_normal(md * pow5_exact[(proven_size_t)e], &acc2);
        }
    } else {
        proven_i64 e = -decimal->exp10;
        while (e >= 22) {
            md = proven_float_frexp_normal(md / pow5_chunk, &acc2);
            e -= 22;
        }
        if (e > 0) {
            md = proven_float_frexp_normal(md / pow5_exact[(proven_size_t)e], &acc2);
        }
    }

    /* Fold in the 2^exp10 factor: value ~= md x 2^(acc2 + exp10), md in [0.5,1). */
    acc2 += decimal->exp10;

    /* md in [0.5,1) packs as significand (1<<52)|frac with unbiased exp acc2-1. */
    mant_sig = (1ull << 52) | (proven_float_bits_f64(md) & 0x000fffffffffffffull);
    if (proven_float_pack_binary64_candidate(mant_sig, acc2 - 1, bits_out) != PROVEN_FLOAT_FAST_PATH_SUCCESS) {
        return false;
    }
    return true;
}

static proven_err_t proven_float_convert_decimal_impl(const proven_u8 *input, proven_size_t len, double *out,
                                                       proven_float_decimal_stats_t *stats) {
    const proven_u64 max_finite_bits = 0x7fefffffffffffffull;
    proven_float_bigint_t overflow_midpoint;
    proven_float_decimal_number_t decimal;
    proven_u64 floor_bits = 0;
    proven_u64 upper_bits = 0;
    proven_u64 rounded_bits = 0;
    proven_u64 eisel_lemire_bits = 0;
    double fast = 0.0;

    if (!input || len == 0 || !out) {
        return PROVEN_ERR_INVALID_ARG;
    }
    if (!proven_float_decimal_build_number(input, len, &decimal)) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }

    if (stats != 0) {
        ++stats->total_conversions;
    }

    if (decimal.is_zero) {
        if (stats != 0) {
            ++stats->exact_fallback_hits;
        }
        *out = decimal.negative ? proven_float_from_bits(0x8000000000000000ull) : 0.0;
        return PROVEN_OK;
    }

    if (proven_float_try_clinger(&decimal, &fast) == PROVEN_OK) {
        if (stats != 0) {
            ++stats->clinger_fast_path_hits;
        }
        *out = fast;
        return PROVEN_OK;
    }
    {
        proven_float_fast_path_result_t eisel_lemire =
            proven_float_try_eisel_lemire(&decimal, &eisel_lemire_bits, stats);
        if (eisel_lemire == PROVEN_FLOAT_FAST_PATH_SUCCESS) {
            if (stats != 0) {
                ++stats->eisel_lemire_fast_path_hits;
            }
            *out = proven_float_from_bits(decimal.negative ? (eisel_lemire_bits | 0x8000000000000000ull) : eisel_lemire_bits);
            return PROVEN_OK;
        }
    }

    if (stats != 0) {
        ++stats->exact_fallback_hits;
    }

    {
        proven_size_t dropped_digits = 0;
        bool sticky = false;
        if (!proven_float_decimal_build_significand(input, len, &decimal, &decimal.significand,
                                                    &dropped_digits, &sticky)) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        /*
         * Digits dropped past the significand cap shift magnitude into exp10 and
         * shorten the significant-digit count; the kept significand still scales
         * to the same magnitude, and significand_sticky carries the dropped tail.
         */
        decimal.exp10 += (proven_i64)dropped_digits;
        decimal.significant_digits -= dropped_digits;
        decimal.significand_sticky = sticky;
    }

    {
        proven_float_exact_compare_state_t compare_state;
        if (!proven_float_prepare_exact_compare_state(&decimal, &compare_state)) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }

        proven_i64 adjusted_exp10 = decimal.exp10 + (proven_i64)decimal.significant_digits - 1;
        if (adjusted_exp10 > 309) {
            return PROVEN_ERR_OVERFLOW;
        }
        if (adjusted_exp10 < -350) {
            *out = decimal.negative ? proven_float_from_bits(0x8000000000000000ull) : 0.0;
            return PROVEN_OK;
        }

        proven_float_bigint_from_u64(&overflow_midpoint, (1ull << 54) - 1ull);
        if (proven_float_compare_decimal_to_scaled(&decimal, &compare_state, &overflow_midpoint, 970) >= 0) {
            return PROVEN_ERR_OVERFLOW;
        }

        {
            proven_i64 min_exp2 = -1074;
            proven_i64 max_exp2 = 1023;
            proven_u64 lo = 0;
            proven_u64 hi = max_finite_bits;

            proven_float_decimal_binary_exp_bounds(&decimal, decimal.significant_digits, &min_exp2, &max_exp2);

            if (max_exp2 < -1022) {
                lo = 0u;
                hi = 0x000fffffffffffffull;
            } else {
                if (min_exp2 < -1022) {
                    lo = 0u;
                } else {
                    lo = ((proven_u64)(min_exp2 + 1023) << 52);
                }
                if (max_exp2 < -1022) {
                    hi = 0x000fffffffffffffull;
                } else {
                    hi = (((proven_u64)(max_exp2 + 1023) << 52) | 0x000fffffffffffffull);
                }
            }

            /*
             * Seed the search from a cheap double estimate when one is
             * available. The narrowed window is only adopted after verifying
             * it brackets the true floor, so a wrong estimate costs two extra
             * comparisons and a fall-back to the full range, never a wrong
             * result. When the estimate is good this collapses the 52-bit
             * mantissa search to a handful of iterations.
             */
            {
                proven_u64 est_bits = 0;
                if (proven_float_estimate_value_bits(&decimal, &est_bits)) {
                    const proven_u64 window = 16u;
                    proven_u64 cand_lo;
                    proven_u64 cand_hi;

                    if (est_bits < lo) {
                        est_bits = lo;
                    } else if (est_bits > hi) {
                        est_bits = hi;
                    }
                    cand_lo = (est_bits - lo > window) ? est_bits - window : lo;
                    cand_hi = (hi - est_bits > window) ? est_bits + window : hi;

                    if (proven_float_compare_decimal_to_bits(&decimal, &compare_state, cand_lo) >= 0 &&
                        proven_float_compare_decimal_to_bits(&decimal, &compare_state, cand_hi) < 0) {
                        lo = cand_lo;
                        hi = cand_hi;
                    }
                }
            }

            while (lo < hi) {
                proven_u64 mid = lo + ((hi - lo + 1ull) >> 1);
                int cmp = proven_float_compare_decimal_to_bits(&decimal, &compare_state, mid);
                if (cmp >= 0) {
                    lo = mid;
                } else {
                    hi = mid - 1ull;
                }
            }
            floor_bits = lo;
        }

        if (proven_float_compare_decimal_to_bits(&decimal, &compare_state, floor_bits) == 0) {
            rounded_bits = floor_bits;
        } else if (floor_bits == max_finite_bits) {
            rounded_bits = max_finite_bits;
        } else {
            int cmp_mid = 0;
            upper_bits = floor_bits + 1ull;
            cmp_mid = proven_float_compare_decimal_to_midpoint(&decimal, &compare_state, floor_bits, upper_bits);
            if (cmp_mid < 0) {
                rounded_bits = floor_bits;
            } else if (cmp_mid > 0) {
                rounded_bits = upper_bits;
            } else {
                rounded_bits = (floor_bits & 1ull) == 0ull ? floor_bits : upper_bits;
            }
        }
    }

    if (decimal.negative) {
        rounded_bits |= 0x8000000000000000ull;
    }
    *out = proven_float_from_bits(rounded_bits);
    return PROVEN_OK;
}

proven_err_t proven_float_convert_decimal(const proven_u8 *input, proven_size_t len, double *out) {
    return proven_float_convert_decimal_impl(input, len, out, 0);
}

proven_err_t proven_float_convert_decimal_observed(const proven_u8 *input, proven_size_t len, double *out,
                                                   proven_float_decimal_stats_t *stats) {
    return proven_float_convert_decimal_impl(input, len, out, stats);
}

bool proven_float_normalize_scientific(double *abs_v, int *sci_exp) {
    /* Defensive cap: comfortably above the decimal exponent range of double. */
    const int guard_limit = 400;
    int guard = guard_limit;

    while (*abs_v >= 1e256 && guard > 0) {
        *abs_v /= 1e256;
        *sci_exp += 256;
        guard--;
    }
    while (*abs_v >= 1e128 && guard > 0) {
        *abs_v /= 1e128;
        *sci_exp += 128;
        guard--;
    }
    while (*abs_v >= 1e64 && guard > 0) {
        *abs_v /= 1e64;
        *sci_exp += 64;
        guard--;
    }
    while (*abs_v >= 1e32 && guard > 0) {
        *abs_v /= 1e32;
        *sci_exp += 32;
        guard--;
    }
    while (*abs_v >= 1e16 && guard > 0) {
        *abs_v /= 1e16;
        *sci_exp += 16;
        guard--;
    }
    while (*abs_v >= 1e8 && guard > 0) {
        *abs_v /= 1e8;
        *sci_exp += 8;
        guard--;
    }
    while (*abs_v >= 1e4 && guard > 0) {
        *abs_v /= 1e4;
        *sci_exp += 4;
        guard--;
    }
    while (*abs_v >= 1e2 && guard > 0) {
        *abs_v /= 1e2;
        *sci_exp += 2;
        guard--;
    }
    while (*abs_v >= 10.0 && guard > 0) {
        *abs_v /= 10.0;
        (*sci_exp)++;
        guard--;
    }
    while (*abs_v > 0.0 && *abs_v < 1.0 && guard > 0) {
        *abs_v *= 1e256;
        *sci_exp -= 256;
        if (*abs_v >= 1.0) break;
        *abs_v *= 1e128;
        *sci_exp -= 128;
        if (*abs_v >= 1.0) break;
        *abs_v *= 1e64;
        *sci_exp -= 64;
        if (*abs_v >= 1.0) break;
        *abs_v *= 1e32;
        *sci_exp -= 32;
        if (*abs_v >= 1.0) break;
        *abs_v *= 1e16;
        *sci_exp -= 16;
        if (*abs_v >= 1.0) break;
        *abs_v *= 1e8;
        *sci_exp -= 8;
        if (*abs_v >= 1.0) break;
        *abs_v *= 1e4;
        *sci_exp -= 4;
        if (*abs_v >= 1.0) break;
        *abs_v *= 1e2;
        *sci_exp -= 2;
        if (*abs_v >= 1.0) break;
        *abs_v *= 10.0;
        *sci_exp -= 1;
    }
    while (*abs_v >= 10.0 && guard > 0) {
        *abs_v /= 10.0;
        (*sci_exp)++;
        guard--;
    }

    if (guard == 0) {
        return false;
    }

    return true;
}
