#include "proven/float_format.h"
#include "proven/float_config.h"
#include "proven/scan.h"
#include "float_decimal.h"
#include <limits.h>
#include <string.h>

/* Upper bound on decimal digits a full-capacity exact value can produce. */
#define PROVEN_FLOAT_FMT_DIGITS_MAX ((PROVEN_FLOAT_BIGINT_LIMBS * 20u) + 4u)
/* Highest fixed precision the exact path accepts (capacity still bounds it). */
#define PROVEN_FLOAT_FMT_PRECISION_MAX 1100

static int proven_float_format_itoa_raw(unsigned long long val, char *s, int base_in) {
    static const char digits[] = "0123456789abcdef";
    char tmp[64];
    int base = base_in;
    int n = 0;

    if (base != 10 && base != 16) {
        base = 10;
    }

    if (val == 0) {
        if (s) {
            s[0] = '0';
        }
        return 1;
    }

    while (val > 0 && n < (int)sizeof(tmp)) {
        unsigned long long rem = val % (unsigned long long)base;
        tmp[n++] = digits[rem];
        val /= (unsigned long long)base;
    }

    for (int i = 0; i < n; ++i) {
        s[i] = tmp[n - 1 - i];
    }
    return n;
}

/*
 * Formats the shortest significant digits as the shorter of fixed-point and
 * scientific notation (fixed wins ties), matching the documented shortest
 * spelling: scientific has no '+' on a positive exponent and at least two
 * exponent digits. The fixed-form length is computed analytically so an extreme
 * exponent never builds a huge fixed string before discarding it.
 */
static proven_err_t proven_float_format_shortest_from_digits(bool negative, const char *digits, int n,
                                                             proven_i64 dexp, char *buf, proven_size_t buf_cap,
                                                             proven_size_t *written_out) {
    char sci[40];
    proven_size_t sl = 0;
    proven_i64 last = dexp - (proven_i64)(n - 1);
    proven_size_t fixed_len;
    proven_size_t pos = 0;
    int i;

    /* scientific notation (always short) */
    sci[sl++] = digits[0];
    if (n > 1) {
        sci[sl++] = '.';
        for (i = 1; i < n; ++i) {
            sci[sl++] = digits[i];
        }
    }
    sci[sl++] = 'e';
    {
        proven_i64 ex = dexp;
        unsigned long long u;
        char r[20];
        int rl = 0;
        if (ex < 0) {
            sci[sl++] = '-';
            u = (unsigned long long)(-ex);
        } else {
            u = (unsigned long long)ex;
        }
        if (u == 0u) {
            r[rl++] = '0';
        } else {
            while (u != 0u) {
                r[rl++] = (char)('0' + (int)(u % 10u));
                u /= 10u;
            }
        }
        if (rl < 2) {
            sci[sl++] = '0';
        }
        while (rl > 0) {
            sci[sl++] = r[--rl];
        }
    }
    sci[sl] = '\0';

    /* fixed-form length, analytically */
    if (dexp >= 0) {
        fixed_len = (last >= 0) ? (proven_size_t)((proven_i64)n + last) : (proven_size_t)(n + 1);
    } else {
        fixed_len = (proven_size_t)((proven_i64)n + 1 - dexp); /* "0." + (-dexp-1) zeros + n digits */
    }

    if (negative) {
        if (1u >= buf_cap) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        buf[pos++] = '-';
    }

    if (fixed_len <= sl) {
        /* build fixed in place (bounded: it is no longer than the scientific form) */
        if (pos + fixed_len + 1u > buf_cap) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        if (dexp >= 0) {
            if (last >= 0) {
                for (i = 0; i < n; ++i) buf[pos++] = digits[i];
                for (proven_i64 z = 0; z < last; ++z) buf[pos++] = '0';
            } else {
                proven_size_t intd = (proven_size_t)dexp + 1u;
                for (i = 0; i < (int)intd; ++i) buf[pos++] = digits[i];
                buf[pos++] = '.';
                for (i = (int)intd; i < n; ++i) buf[pos++] = digits[i];
            }
        } else {
            buf[pos++] = '0';
            buf[pos++] = '.';
            for (proven_i64 z = 0; z < -dexp - 1; ++z) buf[pos++] = '0';
            for (i = 0; i < n; ++i) buf[pos++] = digits[i];
        }
    } else {
        if (pos + sl + 1u > buf_cap) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        memcpy(buf + pos, sci, sl);
        pos += sl;
    }
    buf[pos] = '\0';
    if (written_out) {
        *written_out = pos;
    }
    return PROVEN_OK;
}

static proven_err_t proven_float_format_build_shortest_f64(char *buf, proven_size_t buf_cap, double value, proven_size_t *written_out) {
    proven_u64 bits = proven_float_bits_f64(value);
    bool negative = (bits >> 63) != 0;
    proven_u64 exp = (bits >> 52) & 0x7ffull;
    char digits[20];
    proven_i64 dexp = 0;
    int n;

    if (!buf) {
        return PROVEN_ERR_INVALID_ARG;
    }
    if (buf_cap == 0u) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    if (exp == 0x7ffull) {
        const char *s = (bits & 0x000fffffffffffffull) ? "NaN" : (negative ? "-Inf" : "Inf");
        proven_size_t len = (proven_size_t)strlen(s);
        if (len + 1u > buf_cap) return PROVEN_ERR_OUT_OF_BOUNDS;
        memcpy(buf, s, len + 1u);
        if (written_out) *written_out = len;
        return PROVEN_OK;
    }
    n = proven_float_shortest_digits(value, digits, sizeof digits, &dexp);
    if (n < 0) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    return proven_float_format_shortest_from_digits(negative, digits, n, dexp, buf, buf_cap, written_out);
}

static proven_err_t proven_float_format_build_shortest_f32(char *buf, proven_size_t buf_cap, float value, proven_size_t *written_out) {
    proven_u32 bits = proven_float_bits_f32(value);
    bool negative = (bits >> 31) != 0;
    proven_u32 exp = (bits >> 23) & 0xffu;
    char digits[16];
    proven_i64 dexp = 0;
    int n;

    if (!buf) {
        return PROVEN_ERR_INVALID_ARG;
    }
    if (buf_cap == 0u) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    if (exp == 0xffu) {
        const char *s = (bits & 0x007fffffu) ? "NaN" : (negative ? "-Inf" : "Inf");
        proven_size_t len = (proven_size_t)strlen(s);
        if (len + 1u > buf_cap) return PROVEN_ERR_OUT_OF_BOUNDS;
        memcpy(buf, s, len + 1u);
        if (written_out) *written_out = len;
        return PROVEN_OK;
    }
    n = proven_float_shortest_digits_f32(value, digits, sizeof digits, &dexp);
    if (n < 0) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    return proven_float_format_shortest_from_digits(negative, digits, n, dexp, buf, buf_cap, written_out);
}

/*
 * Exact `%f`-style fixed-precision formatting: `precision` digits after the
 * decimal point, correctly rounded (round-half-to-even), at arbitrary magnitude
 * and precision. Integer-only via the shared exact engine; no long double, no
 * precision/magnitude ceiling beyond the big-integer capacity.
 */
static proven_err_t proven_float_format_fixed_f_exact(char *buf, proven_size_t buf_cap, double value,
                                                      proven_i32 precision, proven_size_t *written_out) {
    proven_u64 bits = proven_float_bits_f64(value);
    bool sign = (bits >> 63) != 0;
    proven_u64 exp = (bits >> 52) & 0x7ffull;
    proven_u64 frac = bits & 0x000fffffffffffffull;
    proven_size_t pos = 0;
    char digits[PROVEN_FLOAT_FMT_DIGITS_MAX];
    int nd;
    proven_size_t L;
    proven_size_t P;
    proven_size_t i;

    if (!buf || buf_cap == 0u || precision < 0) {
        return PROVEN_ERR_INVALID_ARG;
    }
    if (exp == 0x7ffull) {
        const char *special = frac != 0u ? "NaN" : (sign ? "-Inf" : "Inf");
        proven_size_t len = (proven_size_t)strlen(special);
        if (len + 1u > buf_cap) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        memcpy(buf, special, len + 1u);
        if (written_out) {
            *written_out = len;
        }
        return PROVEN_OK;
    }

    nd = proven_float_scaled_round_digits(value, (proven_i64)precision, digits, sizeof digits);
    if (nd < 0) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    L = (proven_size_t)nd;
    P = (proven_size_t)precision;

#define PROVEN_FMT_PUT(ch) do { if (pos + 1u >= buf_cap) { return PROVEN_ERR_OUT_OF_BOUNDS; } buf[pos++] = (ch); } while (0)
    if (sign) {
        PROVEN_FMT_PUT('-');
    }
    if (L > P) {
        proven_size_t ilen = L - P;
        for (i = 0; i < ilen; ++i) {
            PROVEN_FMT_PUT(digits[i]);
        }
        if (P > 0u) {
            PROVEN_FMT_PUT('.');
            for (i = ilen; i < L; ++i) {
                PROVEN_FMT_PUT(digits[i]);
            }
        }
    } else {
        PROVEN_FMT_PUT('0');
        if (P > 0u) {
            PROVEN_FMT_PUT('.');
            for (i = 0; i < P - L; ++i) {
                PROVEN_FMT_PUT('0');
            }
            for (i = 0; i < L; ++i) {
                PROVEN_FMT_PUT(digits[i]);
            }
        }
    }
#undef PROVEN_FMT_PUT
    if (pos >= buf_cap) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    buf[pos] = '\0';
    if (written_out) {
        *written_out = pos;
    }
    return PROVEN_OK;
}

/*
 * Exact `%e`-style scientific formatting: `precision` digits after the mantissa
 * point (precision + 1 significant digits), correctly rounded (round-half-to-even),
 * with a signed exponent of at least two digits. Integer-only via the shared exact
 * engine.
 */
static proven_err_t proven_float_format_e_exact(char *buf, proven_size_t buf_cap, double value,
                                                proven_i32 precision, bool exp_force_sign, int exp_min_digits,
                                                proven_size_t *written_out) {
    proven_u64 bits = proven_float_bits_f64(value);
    bool sign = (bits >> 63) != 0;
    proven_u64 exp = (bits >> 52) & 0x7ffull;
    proven_u64 frac = bits & 0x000fffffffffffffull;
    proven_size_t pos = 0;
    char digits[PROVEN_FLOAT_FMT_DIGITS_MAX];
    proven_i64 dexp = 0;
    int sig;
    int nd;
    int i;

    if (!buf || buf_cap == 0u || precision < 0) {
        return PROVEN_ERR_INVALID_ARG;
    }
    if (exp == 0x7ffull) {
        const char *special = frac != 0u ? "NaN" : (sign ? "-Inf" : "Inf");
        proven_size_t len = (proven_size_t)strlen(special);
        if (len + 1u > buf_cap) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        memcpy(buf, special, len + 1u);
        if (written_out) {
            *written_out = len;
        }
        return PROVEN_OK;
    }

    sig = precision + 1;
    nd = proven_float_scaled_round_sig_digits(value, sig, digits, sizeof digits, &dexp);
    if (nd < 0) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }

#define PROVEN_FMT_PUT(ch) do { if (pos + 1u >= buf_cap) { return PROVEN_ERR_OUT_OF_BOUNDS; } buf[pos++] = (ch); } while (0)
    if (sign) {
        PROVEN_FMT_PUT('-');
    }
    PROVEN_FMT_PUT(digits[0]);
    if (precision > 0) {
        PROVEN_FMT_PUT('.');
        for (i = 1; i < sig; ++i) {
            PROVEN_FMT_PUT(digits[i]);
        }
    }
    PROVEN_FMT_PUT('e');
    if (dexp < 0) {
        PROVEN_FMT_PUT('-');
    } else if (exp_force_sign) {
        PROVEN_FMT_PUT('+');
    }
    {
        char eb[24];
        unsigned long long uexp = dexp < 0 ? (unsigned long long)(-dexp) : (unsigned long long)dexp;
        int el = proven_float_format_itoa_raw(uexp, eb, 10);
        int pad = exp_min_digits - el;
        while (pad-- > 0) {
            PROVEN_FMT_PUT('0');
        }
        for (i = 0; i < el; ++i) {
            PROVEN_FMT_PUT(eb[i]);
        }
    }
#undef PROVEN_FMT_PUT
    if (pos >= buf_cap) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    buf[pos] = '\0';
    if (written_out) {
        *written_out = pos;
    }
    return PROVEN_OK;
}

static proven_err_t proven_float_format_dispatch_f64(char *buf, proven_size_t buf_cap, double value,
                                                     proven_float_format_policy_t policy,
                                                     proven_float_format_options_t opt,
                                                     proven_size_t *written_out) {
    if (!buf) {
        return PROVEN_ERR_INVALID_ARG;
    }
    if (buf_cap == 0) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    if (policy != PROVEN_FLOAT_FORMAT_POLICY_DEFAULT &&
        policy != PROVEN_FLOAT_FORMAT_POLICY_SIMPLE &&
        policy != PROVEN_FLOAT_FORMAT_POLICY_RYU) {
        return PROVEN_ERR_INVALID_ARG;
    }

    if (opt.mode == PROVEN_FLOAT_FORMAT_MODE_SHORTEST) {
        if (policy == PROVEN_FLOAT_FORMAT_POLICY_RYU) {
            return proven_float_format_build_shortest_f64(buf, buf_cap, value, written_out);
        }
        return PROVEN_ERR_UNSUPPORTED;
    }
    if (opt.mode == PROVEN_FLOAT_FORMAT_MODE_SCIENTIFIC) {
        /* Always scientific, whatever the magnitude - printf %e. The same correctly-rounded
         * core the default form uses at the extremes, just never allowed to fall back to
         * fixed. Signed two-digit-minimum exponent, precision digits after the point. */
        if (opt.precision < 0 || opt.precision > PROVEN_FLOAT_FMT_PRECISION_MAX) {
            return PROVEN_ERR_INVALID_ARG;
        }
        return proven_float_format_e_exact(buf, buf_cap, value, opt.precision, true, 2, written_out);
    }
    if (opt.mode != PROVEN_FLOAT_FORMAT_MODE_FIXED) {
        return PROVEN_ERR_INVALID_ARG;
    }
    if (opt.precision < 0 || opt.precision > PROVEN_FLOAT_FMT_PRECISION_MAX) {
        return PROVEN_ERR_INVALID_ARG;
    }

    /* FIXED mode for any supported policy (DEFAULT/SIMPLE/RYU): exact,
       arbitrary-precision path. Magnitude selects %f vs %e (signed two-digit
       exponent). RYU only differs from the others in SHORTEST mode, handled
       above. */
    {
        double abs_v = value < 0.0 ? -value : value;
        bool use_scientific = (abs_v >= 1e18 || (abs_v > 0.0 && abs_v < 1e-4));
        if (opt.never_scientific) use_scientific = false;
        if (!use_scientific) {
            /* Exact, arbitrary-precision %f path. */
            return proven_float_format_fixed_f_exact(buf, buf_cap, value, opt.precision, written_out);
        }
        /* Exact, arbitrary-precision %e (scientific) path: signed two-digit exponent. */
        return proven_float_format_e_exact(buf, buf_cap, value, opt.precision, true, 2, written_out);
    }
}

proven_err_t proven_float_format_f64_policy(char *buf, proven_size_t buf_cap, double value,
                                            proven_float_format_policy_t policy,
                                            proven_float_format_options_t opt,
                                            proven_size_t *written_out) {
    /*
     * Precision 0 used to be silently rewritten to 6.
     *
     * "No decimals" is a legitimate request - it is what `%.0f` means everywhere -
     * and answering it with six decimals is the same disease as accepting a spec and
     * ignoring it: the caller asked for something, got something else, and was told
     * it worked. A caller who wants the default asks for it by name, with
     * proven_float_format_options_fixed_default().
     */
    return proven_float_format_dispatch_f64(buf, buf_cap, value, policy, opt, written_out);
}

proven_err_t proven_float_format_f32_policy(char *buf, proven_size_t buf_cap, float value,
                                            proven_float_format_policy_t policy,
                                            proven_float_format_options_t opt,
                                            proven_size_t *written_out) {
    if (policy == PROVEN_FLOAT_FORMAT_POLICY_RYU) {
        if (opt.mode != PROVEN_FLOAT_FORMAT_MODE_SHORTEST) {
            return PROVEN_ERR_UNSUPPORTED;
        }
        return proven_float_format_build_shortest_f32(buf, buf_cap, value, written_out);
    }
    return proven_float_format_f64_policy(buf, buf_cap, (double)value, policy, opt, written_out);
}
