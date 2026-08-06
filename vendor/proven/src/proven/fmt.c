#include "proven/fmt.h"
#include "float_decimal.h"
#include "proven/float_format.h"
#include "proven_internal_memrange.h"
#include "../../platform/proven_sys_mem.h"

// =============================================================================
// Formatting Context
// =============================================================================

// Internal helpers for string reversal and integer conversion
static const char digit_pairs[201] = 
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

typedef struct {
    char fill;
    char align;      // '<', '>', '^'
    int  width;
    int  precision;  // -1 when unset
    char type;       // 0, 'x', 'X', 'o', 'b', 'd', 'f', 'g', 'e'
    char sign;       // 0, '+', ' '
    bool alt;        // '#' - 0x / 0o / 0b prefix
    bool hex;        // kept: true when type is 'x' or 'X'
} proven_fmt_spec_t;
 
 typedef struct {
    proven_u8str_t *str;
    proven_err_t    err;
    proven_size_t   written;
    proven_size_t   required;
    bool            measure_only;
} proven_fmt_ctx_t;

// =============================================================================
// Rendering Helpers
// =============================================================================

static void fmt_append_view(proven_fmt_ctx_t *ctx, proven_u8str_view_t view) {
    if (!proven_is_ok(ctx->err)) return;

    if (view.size > 0 && !view.ptr) {
        ctx->err = PROVEN_ERR_INVALID_ARG;
        return;
    }

    if (view.size == 0) return;

    if (PROVEN_CKD_ADD(&ctx->required, ctx->required, view.size)) {
        ctx->err = PROVEN_ERR_OVERFLOW;
        return;
    }

    if (ctx->measure_only) return;

    proven_size_t cap = ctx->str->internal.cap;
    proven_size_t len = ctx->str->internal.len;

    if (len + 1 >= cap) {
        return; // Truncated
    }

    proven_size_t available = cap - len - 1;
    proven_size_t to_copy = view.size;
    if (to_copy > available) {
        to_copy = available;
    }

    proven_sys_mem_move(ctx->str->internal.ptr + len, view.ptr, to_copy);
    ctx->str->internal.len += to_copy;
    ctx->str->internal.ptr[ctx->str->internal.len] = 0;
    ctx->written += to_copy;
}

static void fmt_append_byte(proven_fmt_ctx_t *ctx, proven_u8 b) {
    proven_u8str_view_t v = { .ptr = &b, .size = 1 };
    fmt_append_view(ctx, v);
}

static int itoa_raw(unsigned long long val, char *s, int base_in) {
    if (val == 0) {
        s[0] = '0';
        s[1] = '\0';
        return 1;
    }

    unsigned long long base = (unsigned int)base_in;
    char temp[64];
    int i = 0;
    
    if (base == 10) {
        while (val >= 100) {
            unsigned int rem = (unsigned int)(val % 100);
            val /= 100;
            temp[i++] = digit_pairs[rem * 2 + 1];
            temp[i++] = digit_pairs[rem * 2];
        }
        if (val < 10) {
            temp[i++] = '0' + (char)val;
        } else {
            unsigned int v = (unsigned int)val;
            temp[i++] = digit_pairs[v * 2 + 1];
            temp[i++] = digit_pairs[v * 2];
        }
    } else if (base == 16) {
        static const char hex_chars[] = "0123456789abcdef";
        while (val != 0) {
            temp[i++] = hex_chars[val & 0xFu];
            val >>= 4;
        }
    } else {
        while (val != 0) {
            unsigned int r = (unsigned int)(val % base);
            temp[i++] = (char)((r > 9) ? (char)((r - 10) + 'a') : (char)(r + '0'));
            val /= base;
        }
    }
    
    for (int j = 0; j < i; j++) {
        s[j] = temp[i - 1 - j];
    }
    s[i] = '\0';
    return i;
}

static void append_padding(proven_fmt_ctx_t *ctx, char fill, int count) {
    if (count <= 0) return;
    if (ctx->measure_only) {
        if (PROVEN_CKD_ADD(&ctx->required, ctx->required, (proven_size_t)count)) {
            ctx->err = PROVEN_ERR_OVERFLOW;
        }
        return;
    }

    proven_u8str_view_t v = { .ptr = NULL, .size = (proven_size_t)count };
    
    // We can optimize append_padding to not use fmt_append_view which requires a ptr.
    // Instead we directly manipulate the ctx.
    if (!proven_is_ok(ctx->err)) return;

    if (PROVEN_CKD_ADD(&ctx->required, ctx->required, v.size)) {
        ctx->err = PROVEN_ERR_OVERFLOW;
        return;
    }

    proven_size_t cap = ctx->str->internal.cap;
    proven_size_t len = ctx->str->internal.len;

    if (len + 1 >= cap) return;

    proven_size_t available = cap - len - 1;
    proven_size_t to_write = v.size;
    if (to_write > available) to_write = available;

    proven_sys_mem_set(ctx->str->internal.ptr + len, (int)fill, to_write);
    ctx->str->internal.len += to_write;
    ctx->str->internal.ptr[ctx->str->internal.len] = 0;
    ctx->written += to_write;
}

static void render_with_spec(proven_fmt_ctx_t *ctx, const char *val, proven_size_t val_len, proven_fmt_spec_t spec) {
    if (spec.width == 0 || (proven_size_t)spec.width <= val_len) {
        fmt_append_view(ctx, (proven_u8str_view_t){(const proven_u8*)val, val_len});
        return;
    }

    int total_padding = spec.width - (int)val_len;
    if (spec.align == '>') {
        append_padding(ctx, spec.fill, total_padding);
        fmt_append_view(ctx, (proven_u8str_view_t){(const proven_u8*)val, (proven_size_t)val_len});
    } else if (spec.align == '<') {
        fmt_append_view(ctx, (proven_u8str_view_t){(const proven_u8*)val, (proven_size_t)val_len});
        append_padding(ctx, spec.fill, total_padding);
    } else { // '^'
        int left = total_padding / 2;
        int right = total_padding - left;
        append_padding(ctx, spec.fill, left);
        fmt_append_view(ctx, (proven_u8str_view_t){(const proven_u8*)val, (proven_size_t)val_len});
        append_padding(ctx, spec.fill, right);
    }
}


/* A type letter picks the base. Nothing else does. */
static int spec_base(proven_fmt_spec_t spec) {
    switch (spec.type) {
        case 'x': case 'X': return 16;
        case 'o':           return 8;
        case 'b':           return 2;
        default:            return 10;
    }
}

static const char *spec_prefix(proven_fmt_spec_t spec) {
    if (!spec.alt) return "";
    switch (spec.type) {
        case 'x': return "0x";
        case 'X': return "0X";
        case 'o': return "0o";
        case 'b': return "0b";
        default:  return "";
    }
}

/*
 * One integer renderer for i32/u32/i64/u64.
 *
 * The four cases used to be four near-copies that between them supported exactly one
 * type letter (lowercase 'x') and no sign control at all, so `{:X}`, `{:o}`, `{:b}`,
 * `{:#x}` and `{:+}` did not exist and `{:08}` was silently mis-parsed. Doing it once
 * is what makes them all possible.
 */
static void render_integer(proven_fmt_ctx_t *ctx, unsigned long long mag, bool negative, proven_fmt_spec_t spec) {
    char digits[72];                       /* 64 binary digits + NUL, with room to spare */
    int base = spec_base(spec);
    int n = itoa_raw(mag, digits, base);

    if (spec.type == 'X') {
        for (int i = 0; i < n; ++i) {
            if (digits[i] >= 'a' && digits[i] <= 'f') digits[i] = (char)(digits[i] - 'a' + 'A');
        }
    }

    char sign = 0;
    if (negative)              sign = '-';
    else if (spec.sign == '+') sign = '+';
    else if (spec.sign == ' ') sign = ' ';

    const char *prefix = spec_prefix(spec);
    proven_size_t plen = 0;
    while (prefix[plen]) ++plen;

    /*
     * Zero-padding is EMITTED, not assembled.
     *
     * The first version of this built the whole padded number in a fixed 128-byte
     * buffer, which meant `{:#0200x}` - a perfectly legal request, since the parser
     * allows a width up to 10000 - silently produced 127 characters and returned
     * PROVEN_OK. The width was not honoured and nobody was told. That is the same
     * disease as a spec that is parsed and then ignored, and it is why the padding is
     * streamed through append_padding here instead: append_padding already knows how
     * to write N of something without holding N of it.
     *
     * The zeros go BETWEEN the sign and the digits: `{:+08}` on 42 is "+0000042",
     * never "0000+42". Padding is part of the number, and a number's sign comes first.
     */
    proven_size_t body = (sign ? 1u : 0u) + plen + (proven_size_t)n;

    if (spec.fill == '0' && spec.align == '>' && spec.width > 0 && (proven_size_t)spec.width > body) {
        if (sign) fmt_append_byte(ctx, (proven_u8)sign);
        if (plen) fmt_append_view(ctx, (proven_u8str_view_t){ (const proven_u8 *)prefix, plen });
        append_padding(ctx, '0', spec.width - (int)body);
        fmt_append_view(ctx, (proven_u8str_view_t){ (const proven_u8 *)digits, (proven_size_t)n });
        return;
    }

    /* No zero-fill: assemble sign + prefix + digits (at most 1 + 2 + 64 bytes) and let
     * render_with_spec do the alignment padding. */
    char out[80];
    proven_size_t len = 0;
    if (sign) out[len++] = sign;
    for (proven_size_t i = 0; i < plen; ++i) out[len++] = prefix[i];
    for (int i = 0; i < n; ++i) out[len++] = digits[i];

    render_with_spec(ctx, out, len, spec);
}

/* --- custom arguments ---------------------------------------------------- */

/* A sink that counts and throws away, so a custom renderer's output can be measured
 * before it is emitted. That is what lets width and alignment work on a type the
 * library has never seen, without allocating a scratch buffer for it. */
typedef struct {
    proven_size_t len;
} fmt_count_sink_t;

static proven_err_t fmt_count_sink_write(void *vctx, proven_u8str_view_t chunk) {
    fmt_count_sink_t *s = (fmt_count_sink_t *)vctx;
    if (chunk.size > 0 && !chunk.ptr) return PROVEN_ERR_INVALID_ARG;
    s->len += chunk.size;
    return PROVEN_OK;
}

/* A sink that appends straight into the formatter's own output. */
static proven_err_t fmt_ctx_sink_write(void *vctx, proven_u8str_view_t chunk) {
    proven_fmt_ctx_t *ctx = (proven_fmt_ctx_t *)vctx;
    fmt_append_view(ctx, chunk);
    return ctx->err;
}

static void render_custom(proven_fmt_ctx_t *ctx, proven_arg_custom_t c, proven_fmt_spec_t spec) {
    if (!c.render) {
        ctx->err = PROVEN_ERR_INVALID_ARG;
        return;
    }

    /* Pass one: how long is it? The renderer must be deterministic; the check below
     * holds it to that, because a renderer that answers differently the second time
     * would silently produce a field of the wrong width. */
    fmt_count_sink_t count = { .len = 0 };
    proven_err_t err = c.render((proven_fmt_sink_t){ .ctx = &count, .write = fmt_count_sink_write }, c.obj);
    if (!proven_is_ok(err)) {
        ctx->err = err;
        return;
    }

    int total_padding = 0;
    if (spec.width > 0 && (proven_size_t)spec.width > count.len) {
        total_padding = spec.width - (int)count.len;
    }
    int left = 0, right = 0;
    if (total_padding > 0) {
        if (spec.align == '>')      { left = total_padding; }
        else if (spec.align == '<') { right = total_padding; }
        else                        { left = total_padding / 2; right = total_padding - left; }
    }

    append_padding(ctx, spec.fill, left);

    proven_size_t before = ctx->required;
    err = c.render((proven_fmt_sink_t){ .ctx = ctx, .write = fmt_ctx_sink_write }, c.obj);
    if (!proven_is_ok(err)) {
        if (proven_is_ok(ctx->err)) ctx->err = err;
        return;
    }
    if (!proven_is_ok(ctx->err)) return;

    if (ctx->required - before != count.len) {
        /* The two passes disagreed. Emitting the result would put a field of the wrong
         * width into an aligned column, and the caller would never know. */
        ctx->err = PROVEN_ERR_INVALID_ARG;
        return;
    }

    append_padding(ctx, spec.fill, right);
}

static void render_arg(proven_fmt_ctx_t *ctx, const proven_arg_t *arg, proven_fmt_spec_t spec) {
    if (!proven_is_ok(ctx->err)) return;

    /*
     * A spec the argument cannot honour is a mistake, not a suggestion.
     *
     * `{:x}` on a double used to print "3.500000" and on a string used to print the
     * string - the request was read, then dropped on the floor, with no error. The
     * caller asked for something and got something else while being told it worked.
     */
    bool is_int = (arg->type == PROVEN_ARG_I32 || arg->type == PROVEN_ARG_U32 ||
                   arg->type == PROVEN_ARG_I64 || arg->type == PROVEN_ARG_U64);
#ifndef PROVEN_FMT_NO_FLOAT
    bool is_float = (arg->type == PROVEN_ARG_F64);
#else
    /* No float support compiled in: nothing can honour a float spec, so every float
     * form is refused rather than silently ignored. */
    bool is_float = false;
#endif

    if (arg->type == PROVEN_ARG_CUSTOM && (spec.type != 0 || spec.precision >= 0 || spec.alt || spec.sign)) {
        /* The library does not know what {:x} or {:.2} would mean for your type. Making
         * something up and reporting success is exactly the failure this guard exists
         * to prevent - it just happens to be your type instead of a double. */
        ctx->err = PROVEN_ERR_INVALID_FORMAT;
        return;
    }

    switch (spec.type) {
        case 0: break;
        case 'x': case 'X': case 'o': case 'b': case 'd':
            if (!is_int) { ctx->err = PROVEN_ERR_INVALID_FORMAT; return; }
            break;
        case 'f': case 'g': case 'e':
            if (!is_float) { ctx->err = PROVEN_ERR_INVALID_FORMAT; return; }
            break;
        default:
            ctx->err = PROVEN_ERR_INVALID_FORMAT;
            return;
    }
    if (spec.alt && !is_int) { ctx->err = PROVEN_ERR_INVALID_FORMAT; return; }
    if (spec.sign && !(is_int || is_float)) { ctx->err = PROVEN_ERR_INVALID_FORMAT; return; }
    if (spec.precision >= 0 && !is_float) {
        /* Precision on an integer or a string has no meaning here, and guessing one
         * would be worse than refusing. */
        ctx->err = PROVEN_ERR_INVALID_FORMAT;
        return;
    }

    /*
     * 128 bytes cover every non-float rendering: sign, digits, decimal point, exponent
     * text, NUL.
     *
     * The FIXED form of a float does not fit in that, and cannot be made to: 1e308 written
     * out in full is 309 integer digits, and the parser accepts a precision of up to 1100
     * on top. The first version of `{:f}` rendered into this buffer and reported anything
     * larger as PROVEN_ERR_INVALID_FORMAT - a *bad format string* - so `{:f}` on 1e121 was
     * indistinguishable from a typo in the spec, and no output buffer the caller supplied
     * could make it work. The float case gets scratch sized for what it is actually asked
     * to produce.
     */
    char buf[128];
    proven_size_t len = 0;

    switch (arg->type) {
        case PROVEN_ARG_I32: {
            long long v = arg->value.i32;
            bool neg = (v < 0) && spec_base(spec) == 10;
            unsigned long long mag = neg ? (unsigned long long)(-(v + 1)) + 1ull
                                         : (unsigned long long)(unsigned int)arg->value.i32;
            /* In base 10 a negative number gets a '-' and its magnitude. In base 16,
             * 8 or 2 it is shown as its bit pattern - that is what a caller asking
             * for hex wants to see. */
            if (spec_base(spec) == 10 && v >= 0) mag = (unsigned long long)v;
            render_integer(ctx, mag, neg, spec);
            break;
        }
        case PROVEN_ARG_U32:
            render_integer(ctx, arg->value.u32, false, spec);
            break;
        case PROVEN_ARG_I64: {
            long long v = arg->value.i64;
            bool neg = (v < 0) && spec_base(spec) == 10;
            unsigned long long mag = neg ? (unsigned long long)(-(v + 1)) + 1ull
                                         : (unsigned long long)v;
            render_integer(ctx, mag, neg, spec);
            break;
        }
        case PROVEN_ARG_U64:
            render_integer(ctx, arg->value.u64, false, spec);
            break;
#ifndef PROVEN_FMT_NO_FLOAT
        case PROVEN_ARG_F64: {
            /*
             * The exact engine has always been able to do precision, fixed form and
             * shortest round-trip. The `{}` grammar simply could not reach it: every
             * float came out with exactly six decimals, forever, which is why a float
             * column could not be aligned - 12.5 rendered nine characters wide and
             * 100.0 rendered ten.
             */
            proven_float_format_options_t opt = proven_float_format_options_fixed_default();
            proven_float_format_policy_t policy = PROVEN_FLOAT_FORMAT_POLICY_DEFAULT;

            if (spec.type == 'g') {
                opt = proven_float_format_options_shortest();
                policy = PROVEN_FLOAT_FORMAT_POLICY_RYU;
            } else if (spec.type == 'e') {
                /* Always scientific, printf %e: precision digits after the point (default 6). */
                opt.mode = PROVEN_FLOAT_FORMAT_MODE_SCIENTIFIC;
                opt.precision = (spec.precision >= 0) ? spec.precision : 6;
                policy = PROVEN_FLOAT_FORMAT_POLICY_SIMPLE;
            } else if (spec.precision >= 0 || spec.type == 'f') {
                opt.mode = PROVEN_FLOAT_FORMAT_MODE_FIXED;
                opt.precision = (spec.precision >= 0) ? spec.precision : 6;
                if (spec.type == 'f') {
                    /* `%f` means no exponent, ever. Without this, `{:f}` on 1e20 gave
                     * "1.000000e+20" - the scientific form it was asked to avoid -
                     * and `{}` and `{:f}` were byte-identical for every input. */
                    policy = PROVEN_FLOAT_FORMAT_POLICY_SIMPLE;
                    opt.never_scientific = true;
                }
            }

            /*
             * Scratch big enough for the widest thing the grammar can ask for: the fixed
             * form of 1e308 is 309 integer digits, the spec allows up to 60 decimals, plus
             * a sign, a point and a NUL. (The engine itself goes to 1100 decimals; the
             * grammar does not, and says so with PROVEN_ERR_OUT_OF_BOUNDS.)
             *
             * `{:f}` used to render into the shared 128-byte buffer and report anything
             * longer as PROVEN_ERR_INVALID_FORMAT - a *bad format string*. So `{:f}` on
             * 1e121 looked exactly like a typo in the spec, and no output buffer the caller
             * supplied could make it work. The number was fine; the scratch was not.
             */
            char fbuf[1536];
            proven_size_t flen = 0;
            proven_err_t ferr = proven_float_format_f64_policy(fbuf, sizeof fbuf, arg->value.f64,
                                                               policy, opt, &flen);
            if (ferr != PROVEN_OK) {
                /* Out of room is not a malformed spec. Say which it was. */
                ctx->err = (ferr == PROVEN_ERR_OUT_OF_BOUNDS) ? PROVEN_ERR_OUT_OF_BOUNDS
                                                              : PROVEN_ERR_INVALID_FORMAT;
                break;
            }

            /* A sign was asked for and the value is not negative: add it. */
            if (spec.sign && fbuf[0] != '-') {
                if (flen + 2u > sizeof fbuf) {
                    ctx->err = PROVEN_ERR_OUT_OF_BOUNDS;
                    break;
                }
                for (proven_size_t i = flen; i > 0; --i) fbuf[i] = fbuf[i - 1];
                fbuf[0] = spec.sign;
                flen += 1;
            }

            /*
             * Zero-fill goes BETWEEN the sign and the digits, exactly as it does for an
             * integer: `{:08.2f}` on -3.14 is "-0003.14", never "000-3.14", which is not a
             * number a reader or a numeric column can make sense of. render_with_spec pads
             * the whole string, so the sign has to be lifted out and the zeros placed after
             * it here - the same fix render_integer already carries.
             */
            if (spec.fill == '0' && spec.align == '>' && spec.width > 0 &&
                (proven_size_t)spec.width > flen) {
                proven_size_t sign_len = (flen > 0 && (fbuf[0] == '-' || fbuf[0] == '+' || fbuf[0] == ' ')) ? 1u : 0u;
                if (sign_len) fmt_append_byte(ctx, (proven_u8)fbuf[0]);
                append_padding(ctx, '0', spec.width - (int)flen);
                fmt_append_view(ctx, (proven_u8str_view_t){ (const proven_u8 *)fbuf + sign_len, flen - sign_len });
                break;
            }

            render_with_spec(ctx, fbuf, flen, spec);
            break;
        }
#endif
        case PROVEN_ARG_CHAR: {
            char c = arg->value.c;
            render_with_spec(ctx, &c, 1, spec);
            break;
        }
        case PROVEN_ARG_BOOL:
            if (arg->value.b) render_with_spec(ctx, "true", 4, spec);
            else              render_with_spec(ctx, "false", 5, spec);
            break;
        case PROVEN_ARG_CUSTOM:
            render_custom(ctx, arg->value.custom, spec);
            break;
        case PROVEN_ARG_CSTR:
            render_with_spec(ctx, arg->value.cstr, (proven_size_t)proven_cstr_len(arg->value.cstr), spec);
            break;
        case PROVEN_ARG_STR_VIEW:
            render_with_spec(ctx, (const char*)arg->value.str_view.ptr, arg->value.str_view.size, spec);
            break;
        case PROVEN_ARG_DATETIME: {
            proven_datetime_t dt = arg->value.datetime;
            char *curr = buf;

            /* itoa_raw writes n digits plus a NUL, so the scratch must hold
             * ULLONG_MAX (20 digits) and its terminator. */
            #define APPEND_FIXED(val, digits) { \
                char temp[24]; \
                int n = itoa_raw((unsigned long long)(val), temp, 10); \
                for (int _i = 0; _i < (digits) - n; _i++) *curr++ = '0'; \
                for (int _i = 0; _i < n; _i++) *curr++ = temp[_i]; \
            }

            /* year is the one signed field. Casting it straight to unsigned
             * long long turned -1 into 18446744073709551615: twenty digits of
             * nonsense, and twenty digits plus a NUL into a twenty-byte
             * scratch buffer. Render the sign, then the magnitude. Negating
             * through long long keeps INT32_MIN in range. */
            {
                char temp[24];
                proven_i32 y = dt.year;
                unsigned long long mag = (y < 0)
                    ? (unsigned long long)(-(long long)y)
                    : (unsigned long long)y;
                if (y < 0) *curr++ = '-';
                int n = itoa_raw(mag, temp, 10);
                for (int _i = 0; _i < 4 - n; _i++) *curr++ = '0';
                for (int _i = 0; _i < n; _i++) *curr++ = temp[_i];
            }
            *curr++ = '-';
            APPEND_FIXED(dt.month, 2); *curr++ = '-';
            APPEND_FIXED(dt.day, 2); *curr++ = ' ';
            APPEND_FIXED(dt.hour, 2); *curr++ = ':';
            APPEND_FIXED(dt.min, 2); *curr++ = ':';
            APPEND_FIXED(dt.sec, 2);
            *curr = '\0';
            len = (proven_size_t)(curr - buf);
            #undef APPEND_FIXED
            
            render_with_spec(ctx, buf, len, spec);
            break;
        }
        case PROVEN_ARG_PTR: {
            buf[0] = '0'; buf[1] = 'x';
            len = (proven_size_t)itoa_raw((unsigned long long)(proven_uintptr_t)arg->value.ptr, buf + 2, 16) + 2;
            render_with_spec(ctx, buf, len, spec);
            break;
        }
        case PROVEN_ARG_FN: {
            const unsigned char *p = (const unsigned char *)&arg->value.fn;
            proven_size_t s = sizeof(arg->value.fn);
            char *curr = buf;
            // Respect ISO C: function pointers are not data pointers.
            // We format the raw representation bytes.
            *curr++ = '('; *curr++ = 'f'; *curr++ = 'n'; *curr++ = ':';
            *curr++ = '0'; *curr++ = 'x';
            for (proven_size_t i = 0; i < s; i++) {
                static const char hex_map[] = "0123456789abcdef";
                unsigned char b = p[i];
                *curr++ = hex_map[b >> 4];
                *curr++ = hex_map[b & 0xf];
            }
            *curr++ = ')';
            *curr = '\0';
            len = (proven_size_t)(curr - buf);
            render_with_spec(ctx, buf, len, spec);
            break;
        }
        default:
            ctx->err = PROVEN_ERR_INVALID_ARG;
            break;
    }
}

// =============================================================================
// Parsing Helpers
// =============================================================================

static bool proven_fmt_parse_arg_index(const char **p_ptr, proven_size_t *out_idx, proven_err_t *err) {
    const char *p = *p_ptr;
    if (*p >= '0' && *p <= '9') {
        proven_size_t arg_idx_raw = 0;
        while (*p >= '0' && *p <= '9') { 
            proven_size_t digit = (proven_size_t)(*p - '0');
            if (PROVEN_CKD_MUL(&arg_idx_raw, arg_idx_raw, 10) ||
                PROVEN_CKD_ADD(&arg_idx_raw, arg_idx_raw, digit)) {
                *err = PROVEN_ERR_OVERFLOW;
                return true;
            }
            p++; 
        }
        *p_ptr = p;
        *out_idx = arg_idx_raw;
        return true;
    }
    return false;
}

// =============================================================================
// Format Engine
// =============================================================================

static void fmt_run(proven_fmt_ctx_t *ctx, const char *fmt, const proven_arg_t *args, proven_size_t args_count, proven_size_t *max_idx) {
    const char *p = fmt;
    proven_size_t next_arg_idx = 1;
    *max_idx = 0;

    while (*p && proven_is_ok(ctx->err)) {
        if (*p == '}') {
            p++;
            if (*p == '}') {
                fmt_append_byte(ctx, (proven_u8)'}');
                p++; continue;
            }
            // Bare '}' is an error in some specs, but let's be strict.
            ctx->err = PROVEN_ERR_INVALID_FORMAT;
            return;
        }

        if (*p != '{') {
            /* Take the whole literal run in one go. Feeding it a character at a
             * time cost a checked add, an out-of-line one-byte move and a NUL
             * reseal per character - and the whole format runs twice, once to
             * measure and once to write. */
            const char *run = p;
            do { ++p; } while (*p && *p != '{' && *p != '}');
            proven_u8str_view_t lit = {
                .ptr = (const proven_byte_t *)run,
                .size = (proven_size_t)(p - run)
            };
            fmt_append_view(ctx, lit);
            continue;
        }

        p++; // skip {
        if (*p == '{') {
            fmt_append_byte(ctx, (proven_u8)'{');
            p++; continue;
        }

        proven_size_t arg_idx_raw = 0;
        bool has_idx = proven_fmt_parse_arg_index(&p, &arg_idx_raw, &ctx->err);
        if (!proven_is_ok(ctx->err)) return;

        proven_size_t arg_idx = PROVEN_INDEX_NOT_FOUND;
        if (has_idx) {
            proven_size_t mapped_idx;
            if (PROVEN_CKD_ADD(&mapped_idx, arg_idx_raw, 1)) {
                ctx->err = PROVEN_ERR_OVERFLOW;
                return;
            }
            arg_idx = mapped_idx;
        } else if (*p == ':' || *p == '}') {
            arg_idx = next_arg_idx++;
        } else {
            ctx->err = PROVEN_ERR_INVALID_FORMAT;
            return;
        }

        if (arg_idx != PROVEN_INDEX_NOT_FOUND && arg_idx > *max_idx) *max_idx = arg_idx;

        proven_fmt_spec_t spec = { .fill = ' ', .align = '>', .width = 0, .precision = -1, .type = 0, .sign = 0, .alt = false, .hex = false };
        if (*p == ':') {
            p++;
            // Check for fill + align
            if (*p && p[0] != '}' && (p[1] == '<' || p[1] == '>' || p[1] == '^')) {
                spec.fill = *p;
                spec.align = p[1];
                p += 2;
            } else if (*p == '<' || *p == '>' || *p == '^') {
                spec.align = *p;
                p++;
            }

            /* Sign, then alternate form, then zero-fill, then width - the order the
             * rest of the world uses, so a spec copied from anywhere else means here
             * what it means there. */
            if (*p == '+' || *p == ' ') {
                spec.sign = *p;
                ++p;
            }
            if (*p == '#') {
                spec.alt = true;
                ++p;
            }

            /* A leading zero is zero-padding, not the first digit of the width.
             *
             * `{:08}` is what every C, Python and Rust programmer writes, and it
             * used to be accepted and silently WRONG: the '0' was eaten as a width
             * digit, so `{:08}` on 42 produced "      42" - space-padded - with no
             * error. A near-miss spelling that is accepted and quietly does the
             * wrong thing is worse than one that is rejected.
             *
             * An explicit fill/align wins, so `{:*>08}` still pads with '*'. */
            if (*p == '0' && spec.fill == ' ' && spec.align == '>') {
                spec.fill = '0';
                ++p;
            }

            int limit = 10000;
            while (*p >= '0' && *p <= '9') {
                int digit = (*p - '0');
                if (PROVEN_CKD_MUL(&spec.width, spec.width, 10)) { ctx->err = PROVEN_ERR_OVERFLOW; return; }
                if (PROVEN_CKD_ADD(&spec.width, spec.width, digit)) { ctx->err = PROVEN_ERR_OVERFLOW; return; }
                if (spec.width > limit) { ctx->err = PROVEN_ERR_OUT_OF_BOUNDS; return; }
                p++;
            }

            /* Precision: `.N`. This is what makes a float column alignable - without
             * it every float was six decimals wide no matter what, so 12.5 took nine
             * characters and 100.0 took ten, and the column simply broke. */
            if (*p == '.') {
                ++p;
                if (!(*p >= '0' && *p <= '9')) {
                    ctx->err = PROVEN_ERR_INVALID_FORMAT;
                    return;
                }
                spec.precision = 0;
                while (*p >= '0' && *p <= '9') {
                    int digit = (*p - '0');
                    if (PROVEN_CKD_MUL(&spec.precision, spec.precision, 10)) { ctx->err = PROVEN_ERR_OVERFLOW; return; }
                    if (PROVEN_CKD_ADD(&spec.precision, spec.precision, digit)) { ctx->err = PROVEN_ERR_OVERFLOW; return; }
                    if (spec.precision > 60) { ctx->err = PROVEN_ERR_OUT_OF_BOUNDS; return; }
                    ++p;
                }
            }

            /* The type letter. */
            switch (*p) {
                case 'x': case 'X': case 'o': case 'b': case 'd': case 'f': case 'g': case 'e':
                    spec.type = *p;
                    spec.hex = (*p == 'x' || *p == 'X');
                    ++p;
                    break;
                default:
                    break;
            }

            // If we are not at '}', it's an invalid specifier character
            if (*p != '}') {
                ctx->err = PROVEN_ERR_INVALID_FORMAT;
                return;
            }
        }

        if (*p != '}') {
            ctx->err = PROVEN_ERR_INVALID_FORMAT;
            return;
        }
        p++; // skip }

        if (arg_idx != PROVEN_INDEX_NOT_FOUND && arg_idx > 0 && (size_t)arg_idx < args_count) {
            render_arg(ctx, &args[arg_idx], spec);
        } else {
            ctx->err = PROVEN_ERR_INVALID_ARG;
        }
    }
}

// =============================================================================
// Alias Patching Helpers
// =============================================================================

typedef struct {
    const proven_arg_t *final_args;
    proven_arg_t *heap_patched;
    proven_arg_t stack_patched[16];
    proven_size_t args_count;
    proven_allocator_t patch_alloc;
} proven_fmt_arg_patch_t;

static proven_err_t proven_fmt_arg_patch_prepare(
    proven_fmt_arg_patch_t *patch,
    const proven_arg_t *args,
    proven_size_t args_count,
    const proven_byte_t *old_ptr,
    proven_size_t old_cap,
    proven_allocator_t patch_alloc)
{
    patch->final_args = args;
    patch->heap_patched = NULL;
    patch->args_count = args_count;
    patch->patch_alloc = patch_alloc;

    bool will_need_patch = false;

    // To guarantee failure-atomicity, pre-allocate heap_patched before string reallocation if an alias is detected
    for (proven_size_t i = 0; i < args_count; i++) {
        if (args[i].type == PROVEN_ARG_STR_VIEW) {
            proven_bufref_t alias_ref = proven_bufref_capture(old_ptr, old_cap, args[i].value.str_view.ptr, args[i].value.str_view.size);
            if (alias_ref.valid) {
                will_need_patch = true;
                break;
            }
        }
    }

    if (will_need_patch && args_count > 16) {
        proven_size_t patch_bytes;
        if (PROVEN_CKD_MUL(&patch_bytes, (proven_size_t)args_count, sizeof(proven_arg_t))) {
            return PROVEN_ERR_OVERFLOW;
        }
        proven_result_mem_mut_t m = patch_alloc.alloc_fn(patch_alloc.ctx, patch_bytes, alignof(proven_arg_t));
        if (!PROVEN_IS_OK(m.err)) {
            return m.err;
        }
        patch->heap_patched = (proven_arg_t*)m.value.ptr;
    }

    return PROVEN_OK;
}

static proven_err_t proven_fmt_arg_patch_apply(
    proven_fmt_arg_patch_t *patch,
    const proven_arg_t *args,
    const proven_byte_t *old_ptr,
    proven_size_t old_cap,
    const proven_byte_t *new_ptr)
{
    patch->final_args = args;
    
    bool needs_patch = (old_ptr != NULL && old_ptr != new_ptr && patch->args_count > 0);
    if (!needs_patch) {
        return PROVEN_OK;
    }

    bool found_any_alias = false;
    for (proven_size_t i = 0; i < patch->args_count; i++) {
        if (args[i].type == PROVEN_ARG_STR_VIEW) {
            proven_bufref_t alias_ref = proven_bufref_capture(old_ptr, old_cap, args[i].value.str_view.ptr, args[i].value.str_view.size);
            if (alias_ref.valid) {
                found_any_alias = true;
                break;
            }
        }
    }

    if (found_any_alias) {
        if (patch->args_count > 16 && !patch->heap_patched) {
            return PROVEN_ERR_NOMEM;
        }

        proven_arg_t *patched_args = patch->heap_patched ? patch->heap_patched : patch->stack_patched;
        for (proven_size_t i = 0; i < patch->args_count; i++) {
            patched_args[i] = args[i];
            if (args[i].type == PROVEN_ARG_STR_VIEW) {
                proven_bufref_t alias_ref = proven_bufref_capture(old_ptr, old_cap, args[i].value.str_view.ptr, args[i].value.str_view.size);
                if (alias_ref.valid) {
                    patched_args[i].value.str_view.ptr = (const proven_byte_t *)proven_bufref_rebase_const(alias_ref, new_ptr);
                }
            }
        }
        patch->final_args = patched_args;
    }

    return PROVEN_OK;
}

static void proven_fmt_arg_patch_destroy(proven_fmt_arg_patch_t *patch) {
    if (patch->heap_patched && proven_alloc_is_valid(patch->patch_alloc)) {
        patch->patch_alloc.free_fn(patch->patch_alloc.ctx, patch->heap_patched);
        patch->heap_patched = NULL;
    }
}

// =============================================================================
// Entry Points
// =============================================================================

proven_fmt_result_t proven_u8str_fmt_internal(proven_allocator_t alloc, proven_u8str_t *str, bool trunc, const char *fmt, proven_allocator_t scratch, const proven_arg_t *args, proven_size_t args_count) {
    proven_fmt_result_t res = { .err = PROVEN_OK };
    if (!str || !fmt || (args_count > 0 && !args)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    // Pass 0: Global self-alias validation for CSTR.
    // CSTR self-alias check (Option A: Explicitly reject as per docs)
    for (proven_size_t i = 0; i < args_count; i++) {
        if (args[i].type == PROVEN_ARG_CSTR) {
            proven_bufref_t alias_ref = proven_bufref_capture(str->internal.ptr, str->internal.cap, args[i].value.cstr, 0);
            if (alias_ref.valid) {
                res.err = PROVEN_ERR_INVALID_ARG;
                return res;
            }
        }
    }

    const proven_byte_t *old_ptr = str->internal.ptr;
    proven_size_t old_cap = str->internal.cap;
    proven_size_t old_len = str->internal.len;
    proven_fmt_arg_patch_t patch = {0};

    if (trunc && !proven_alloc_is_valid(alloc)) {
        proven_fmt_ctx_t write_ctx = {
            .str = str,
            .err = PROVEN_OK,
            .written = 0,
            .required = 0,
            .measure_only = false
        };
        proven_size_t max_idx = 0;
        fmt_run(&write_ctx, fmt, args, args_count, &max_idx);
        proven_size_t produced_required = write_ctx.required;
        proven_size_t produced_written = write_ctx.written;
        res.err = write_ctx.err;

        if (proven_is_ok(res.err)) {
            proven_size_t expected_args;
            if (PROVEN_CKD_ADD(&expected_args, max_idx, 1)) {
                res.err = PROVEN_ERR_OVERFLOW;
            } else if (expected_args != args_count) {
                res.err = PROVEN_ERR_INVALID_ARG;
            }
        }

        if (!proven_is_ok(res.err)) {
            if (old_len < old_cap) {
                str->internal.ptr[old_len] = 0;
            }
            res.required = 0;
            res.written = 0;
            return res;
        }

        res.required = produced_required;
        res.written = produced_written;
        if (trunc && res.written < res.required) {
            res.err = PROVEN_ERR_OUT_OF_BOUNDS;
        }
        return res;
    }

    /*
     * Fixed capacity and atomic: one pass, not two.
     *
     * This is the allocation-free path - PROVEN_FMT into a borrowed stack buffer, the
     * one a logger uses - and it was formatting every value TWICE: once to measure, and
     * then again to write. For a double that means running the correctly-rounded decimal
     * engine twice and throwing the first answer away, which is why formatting a float
     * cost four times what printf's does.
     *
     * It does not need two passes. fmt_append_view already counts every byte the format
     * WANTS (`required`) while copying only the bytes that fit, so one write pass yields
     * both numbers. If the output did not fit, or the format was bad, or the argument
     * count was wrong, we put the length back where it was: the caller's string is
     * unchanged, which is exactly what atomic promised. (Bytes past `len` are scratch;
     * nothing may read them.)
     *
     * The alias-patch machinery below is skipped because it cannot apply: it exists to
     * rebase argument views that pointed into a buffer the allocator moved, and here
     * there is no allocator and nothing moves.
     */
    if (!trunc && !proven_alloc_is_valid(alloc)) {
        proven_fmt_ctx_t one = {
            .str = str, .err = PROVEN_OK, .written = 0, .required = 0, .measure_only = false
        };
        proven_size_t one_max_idx = 0;
        fmt_run(&one, fmt, args, args_count, &one_max_idx);

        res.err = one.err;
        if (proven_is_ok(res.err)) {
            proven_size_t expected;
            if (PROVEN_CKD_ADD(&expected, one_max_idx, 1)) {
                res.err = PROVEN_ERR_OVERFLOW;
            } else if (expected != args_count) {
                res.err = PROVEN_ERR_INVALID_ARG;
            } else if (one.written < one.required) {
                res.err = PROVEN_ERR_OUT_OF_BOUNDS;
            }
        }

        if (!proven_is_ok(res.err)) {
            str->internal.len = old_len;
            if (str->internal.ptr && old_len < str->internal.cap) {
                str->internal.ptr[old_len] = 0;
            }
            res.required = one.required;   /* so a caller can size a buffer and retry */
            res.written = 0;
            return res;
        }

        res.required = one.required;
        res.written = one.written;
        return res;
    }

    // Pass 1: Measure required length
    proven_fmt_ctx_t measure_ctx = {
        .str = str,
        .err = PROVEN_OK,
        .written = 0,
        .required = 0,
        .measure_only = true
    };
    proven_size_t max_idx = 0;
    fmt_run(&measure_ctx, fmt, args, args_count, &max_idx);
    if (!proven_is_ok(measure_ctx.err)) {
        res.err = measure_ctx.err;
        return res;
    }

    // Verification-oriented policy: detection of excess arguments
    proven_size_t expected_args;
    if (PROVEN_CKD_ADD(&expected_args, max_idx, 1)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    
    if (expected_args != args_count) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    res.required = measure_ctx.required;

    // Growth logic
    if (proven_alloc_is_valid(alloc)) {
        proven_size_t total_needed;
        if (PROVEN_CKD_ADD(&total_needed, str->internal.len, measure_ctx.required)) return (proven_fmt_result_t){.err = PROVEN_ERR_OVERFLOW};
        if (PROVEN_CKD_ADD(&total_needed, total_needed, 1)) return (proven_fmt_result_t){.err = PROVEN_ERR_OVERFLOW};

        if (total_needed > str->internal.cap) {
            proven_size_t new_cap = str->internal.cap == 0 ? 16 : str->internal.cap;
            while (new_cap < total_needed) {
                if (PROVEN_CKD_MUL(&new_cap, new_cap, 2)) {
                    new_cap = total_needed;
                    break;
                }
            }
            proven_allocator_t patch_alloc = proven_alloc_is_valid(scratch) ? scratch : alloc;
            proven_err_t patch_err = proven_fmt_arg_patch_prepare(&patch, args, args_count, old_ptr, old_cap, patch_alloc);
            if (!PROVEN_IS_OK(patch_err)) {
                res.err = patch_err;
                return res;
            }

            proven_result_mem_mut_t new_mem = alloc.realloc_fn(alloc.ctx, str->internal.ptr, str->internal.cap, new_cap, PROVEN_DEFAULT_ALIGNMENT);
            if (!proven_is_ok(new_mem.err)) {
                proven_fmt_arg_patch_destroy(&patch);
                res.err = new_mem.err;
                return res;
            }
            str->internal.ptr = (proven_byte_t*)new_mem.value.ptr;
            str->internal.cap = new_cap;
            /* Seal the terminator now. A format that produces no output writes
             * nothing below, so on a freshly allocated (never zeroed) block
             * ptr[len] would otherwise stay uninitialized. */
            str->internal.ptr[str->internal.len] = 0;
        }
    }

    // Atomic fixed-capacity check
    if (!trunc && !proven_alloc_is_valid(alloc)) {
        proven_size_t needed;
        if (PROVEN_CKD_ADD(&needed, str->internal.len, res.required) ||
            PROVEN_CKD_ADD(&needed, needed, 1)) {
            res.err = PROVEN_ERR_OVERFLOW;
            return res;
        }
        if (needed > str->internal.cap) {
            res.err = PROVEN_ERR_OUT_OF_BOUNDS;
            return res;
        }
    }

    // Pass 2: Write
    proven_err_t apply_err = proven_fmt_arg_patch_apply(&patch, args, old_ptr, old_cap, str->internal.ptr);
    if (!PROVEN_IS_OK(apply_err)) {
        res.err = apply_err;
        proven_fmt_arg_patch_destroy(&patch);
        return res;
    }

    proven_fmt_ctx_t write_ctx = {
        .str = str,
        .err = PROVEN_OK,
        .written = 0,
        .required = 0,
        .measure_only = false
    };
    proven_size_t dummy_idx = 0;
    fmt_run(&write_ctx, fmt, patch.final_args, args_count, &dummy_idx);
    
    res.err = write_ctx.err;
    res.written = write_ctx.written;
    
    proven_fmt_arg_patch_destroy(&patch);

    // If we are in trunc mode and wasn't able to write all, signal OOB
    if (trunc && res.written < res.required) {
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
    }

    return res;
}
