#include "proven/float_parse.h"
#include "float_decimal.h"
#include "../../platform/proven_sys_mem.h"

#if !defined(PROVEN_FREESTANDING)
#include <errno.h>
#include <string.h>
#endif

static bool proven_float_parse_is_space(proven_u8 ch) {
    return ch == (proven_u8)' ' || ch == (proven_u8)'\n' || ch == (proven_u8)'\r' ||
           ch == (proven_u8)'\t' || ch == (proven_u8)'\v' || ch == (proven_u8)'\f';
}

static double proven_float_parse_from_bits(proven_u64 bits) {
    double value = 0.0;
    proven_sys_mem_copy(&value, &bits, sizeof value);
    return value;
}

/* A count of bits that saturates well inside proven_i64: a token may hold more digits, or a
 * longer exponent, than any double needs, and past this bound every value is zero or infinite. */
#define PROVEN_FLOAT_HEX_EXP_LIMIT ((proven_i64)1 << 40)

static proven_i64 proven_float_hex_exp_add(proven_i64 exp, proven_i64 delta) {
    exp += delta;
    if (exp > PROVEN_FLOAT_HEX_EXP_LIMIT) return PROVEN_FLOAT_HEX_EXP_LIMIT;
    if (exp < -PROVEN_FLOAT_HEX_EXP_LIMIT) return -PROVEN_FLOAT_HEX_EXP_LIMIT;
    return exp;
}

static int proven_float_hex_digit(proven_u8 ch) {
    if (ch >= (proven_u8)'0' && ch <= (proven_u8)'9') return (int)(ch - (proven_u8)'0');
    if (ch >= (proven_u8)'a' && ch <= (proven_u8)'f') return (int)(ch - (proven_u8)'a') + 10;
    if (ch >= (proven_u8)'A' && ch <= (proven_u8)'F') return (int)(ch - (proven_u8)'A') + 10;
    return -1;
}

/*
 * A hexadecimal float as C's strtod reads it: [sign] 0x hexdigits [. hexdigits] [p [sign] digits],
 * with a digit on at least one side of the point; without the binary exponent the power is 0.
 * Returns false, touching nothing, when input does not start such a token ("0x" with no digit
 * after it is the decimal 0 followed by an x). The value is rounded to nearest, ties to even, from
 * the exact digits: the first sixteen significant ones, and whether any later one is not zero.
 */
static bool proven_float_parse_hex(proven_u8str_view_t input, proven_float_parse_result_t *parsed, double *out_value) {
    const proven_u8 *p = input.ptr;
    proven_size_t len = input.size, cursor = 0;
    bool negative = false, sticky = false;
    proven_u64 mant = 0;
    int kept = 0;
    proven_i64 exp = 0;

    if (cursor < len && (p[cursor] == (proven_u8)'+' || p[cursor] == (proven_u8)'-')) {
        negative = p[cursor] == (proven_u8)'-';
        ++cursor;
    }
    if (len - cursor < 3 || p[cursor] != (proven_u8)'0' || (p[cursor + 1] != (proven_u8)'x' && p[cursor + 1] != (proven_u8)'X')) return false;
    cursor += 2;
    if (proven_float_hex_digit(p[cursor]) < 0 &&
        !(p[cursor] == (proven_u8)'.' && cursor + 1 < len && proven_float_hex_digit(p[cursor + 1]) >= 0)) return false;

    for (bool point = false;; ++cursor) {
        int d;
        if (cursor < len && p[cursor] == (proven_u8)'.' && !point) {
            point = true;
            continue;
        }
        if (cursor >= len || (d = proven_float_hex_digit(p[cursor])) < 0) break;
        if (kept < 16) {
            /* Leading zeros count only for where the point is. */
            if (kept > 0 || d != 0) {
                mant = (mant << 4) | (proven_u64)d;
                ++kept;
            }
            if (point) exp = proven_float_hex_exp_add(exp, -4);
        } else {
            if (d != 0) sticky = true;
            if (!point) exp = proven_float_hex_exp_add(exp, 4);
        }
    }

    if (cursor < len && (p[cursor] == (proven_u8)'p' || p[cursor] == (proven_u8)'P')) {
        proven_size_t at = cursor + 1;
        bool exp_negative = false;
        proven_i64 value = 0;
        if (at < len && (p[at] == (proven_u8)'+' || p[at] == (proven_u8)'-')) {
            exp_negative = p[at] == (proven_u8)'-';
            ++at;
        }
        /* A p with no digits after it is not part of the number, as with the decimal e. */
        if (at < len && p[at] >= (proven_u8)'0' && p[at] <= (proven_u8)'9') {
            for (; at < len && p[at] >= (proven_u8)'0' && p[at] <= (proven_u8)'9'; ++at) {
                if (value < PROVEN_FLOAT_HEX_EXP_LIMIT) value = value * 10 + (proven_i64)(p[at] - (proven_u8)'0');
            }
            exp = proven_float_hex_exp_add(exp, exp_negative ? -value : value);
            cursor = at;
        }
    }

    parsed->kind = PROVEN_FLOAT_PARSE_KIND_DECIMAL;
    parsed->negative = negative;
    parsed->consumed = cursor;
    parsed->has_nonzero_digit = mant != 0;
    parsed->err = PROVEN_OK;

    proven_u64 bits = 0;
    if (mant != 0) {
        /* The value is mant * 2^exp. Keep 53 bits, or fewer below the normal range, where the
         * last kept bit is worth 2^-1074; round the rest to nearest, ties to even. */
        int width = 64;
        while (!(mant >> (width - 1))) --width;
        proven_i64 top = exp + width - 1;
        proven_i64 low = top - 52 < -1074 ? -1074 : top - 52;
        proven_i64 shift = low - exp;
        proven_u64 q = 0;
        if (top > 1023) {
            parsed->err = PROVEN_ERR_OVERFLOW;
        } else if (shift <= 0) {
            q = mant << -shift;
        } else {
            proven_u64 rest, half;
            bool up;
            if (shift > 64) {
                q = 0;
                up = false;
            } else {
                q = shift == 64 ? 0 : mant >> shift;
                rest = shift == 64 ? mant : mant & (((proven_u64)1 << shift) - 1);
                half = (proven_u64)1 << (shift - 1);
                up = rest > half || (rest == half && (sticky || (q & 1)));
            }
            if (up && ++q == ((proven_u64)1 << 53)) {
                q >>= 1;
                ++low;
            }
        }
        if (parsed->err == PROVEN_OK) {
            if (q >= ((proven_u64)1 << 52)) {
                if (low + 1075 >= 2047) parsed->err = PROVEN_ERR_OVERFLOW;
                else bits = ((proven_u64)(low + 1075) << 52) | (q - ((proven_u64)1 << 52));
            } else {
                bits = q;
            }
        }
    }
    if (parsed->err == PROVEN_ERR_OVERFLOW) bits = 0x7ff0000000000000ull;
    if (negative) bits |= 0x8000000000000000ull;
    if (out_value) *out_value = proven_float_parse_from_bits(bits);
    return true;
}

static proven_float_parse_result_t proven_float_parse_double_ascii_internal(proven_u8str_view_t input, double *out_value) {
    proven_float_parse_result_t parsed = { .err = PROVEN_ERR_INVALID_ARG, .kind = PROVEN_FLOAT_PARSE_KIND_DECIMAL, .negative = false, .consumed = 0, .has_nonzero_digit = false };
    double value = 0.0;

    if (input.size > 0 && input.ptr == (const proven_u8 *)0) {
        return parsed;
    }

    if (proven_float_parse_hex(input, &parsed, out_value)) {
        return parsed;
    }

    parsed = proven_float_parse_ascii_token(input.ptr, input.size);
    if (parsed.err != PROVEN_OK) {
        return parsed;
    }

    if (parsed.kind == PROVEN_FLOAT_PARSE_KIND_INF) {
        value = parsed.negative ? proven_float_parse_from_bits(0xfff0000000000000ull)
                                : proven_float_parse_from_bits(0x7ff0000000000000ull);
        if (out_value) {
            *out_value = value;
        }
        return parsed;
    }
    if (parsed.kind == PROVEN_FLOAT_PARSE_KIND_NAN) {
        value = parsed.negative ? proven_float_parse_from_bits(0xfff8000000000000ull)
                                : proven_float_parse_from_bits(0x7ff8000000000000ull);
        if (out_value) {
            *out_value = value;
        }
        return parsed;
    }

    parsed.err = proven_float_convert_decimal(input.ptr, parsed.consumed, &value);
    if (out_value) {
        *out_value = value;
    }
    return parsed;
}

proven_parse_double_result_t proven_parse_double_ascii(proven_u8str_view_t input) {
    proven_parse_double_result_t out = { .err = PROVEN_ERR_INVALID_ARG, .val = 0.0, .consumed = 0 };
    proven_float_parse_result_t parsed = proven_float_parse_double_ascii_internal(input, &out.val);

    if (parsed.err == PROVEN_ERR_OVERFLOW) {
        /* A number, too large for binary64: say so, with how much of the input it took. */
        out.err = PROVEN_ERR_OVERFLOW;
        out.val = parsed.negative ? proven_float_parse_from_bits(0xfff0000000000000ull)
                                  : proven_float_parse_from_bits(0x7ff0000000000000ull);
        out.consumed = parsed.consumed;
        return out;
    }
    if (parsed.err != PROVEN_OK) {
        out.val = 0.0;
        return out;
    }

    out.err = parsed.err;
    out.consumed = parsed.consumed;
    return out;
}

proven_parse_f64_result_t proven_parse_f64_ascii(proven_u8str_view_t input) {
    return proven_parse_double_ascii(input);
}

double proven_strtod(const char *nptr, char **endptr) {
    const proven_u8 *input = (const proven_u8 *)nptr;
    proven_size_t cursor = 0;
    proven_u8str_view_t view;
    proven_float_parse_result_t parsed;
    double result = 0.0;
    bool has_nonzero = false;
    proven_u64 bits = 0;

    if (endptr) {
        *endptr = (char *)nptr;
    }
    if (nptr == (const char *)0) {
        return 0.0;
    }

    while (input[cursor] != (proven_u8)'\0' && proven_float_parse_is_space(input[cursor])) {
        ++cursor;
    }

    view.ptr = input + cursor;
#if !defined(PROVEN_FREESTANDING)
    view.size = (proven_size_t)strlen((const char *)view.ptr);
#else
    view.size = 0;
    while (view.ptr[view.size] != (proven_u8)'\0') {
        ++view.size;
    }
#endif

    parsed = proven_float_parse_double_ascii_internal(view, &result);
    if (parsed.err == PROVEN_ERR_INVALID_ARG || parsed.err == PROVEN_ERR_OUT_OF_BOUNDS) {
        return 0.0;
    }

    if (parsed.consumed != 0) {
        has_nonzero = parsed.has_nonzero_digit;
        if (endptr) {
            *endptr = (char *)(nptr + cursor + parsed.consumed);
        }
    }

    if (parsed.err == PROVEN_OK) {
        proven_sys_mem_copy(&bits, &result, sizeof bits);
        if (has_nonzero && (((bits >> 52) & 0x7ffull) == 0ull)) {
#if !defined(PROVEN_FREESTANDING)
            errno = ERANGE;
#endif
        }
        return result;
    }

    if (parsed.err == PROVEN_ERR_OVERFLOW) {
#if !defined(PROVEN_FREESTANDING)
        errno = ERANGE;
#endif
        return parsed.negative ? proven_float_parse_from_bits(0xfff0000000000000ull)
                               : proven_float_parse_from_bits(0x7ff0000000000000ull);
    }

    return 0.0;
}
