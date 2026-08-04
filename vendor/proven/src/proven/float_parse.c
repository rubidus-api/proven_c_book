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

static proven_float_parse_result_t proven_float_parse_double_ascii_internal(proven_u8str_view_t input, double *out_value) {
    proven_float_parse_result_t parsed = { .err = PROVEN_ERR_INVALID_ARG, .kind = PROVEN_FLOAT_PARSE_KIND_DECIMAL, .negative = false, .consumed = 0, .has_nonzero_digit = false };
    double value = 0.0;

    if (input.size > 0 && input.ptr == (const proven_u8 *)0) {
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

    if (parsed.err != PROVEN_OK) {
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
