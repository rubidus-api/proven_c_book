#include "proven/scan.h"
#include "float_decimal.h"
#include "../../platform/proven_sys_mem.h"
#include <limits.h>

static bool is_whitespace(proven_u8 c) {
    return c == (proven_u8)' ' || c == (proven_u8)'\n' || c == (proven_u8)'\r' || 
           c == (proven_u8)'\t' || c == (proven_u8)'\v' || c == (proven_u8)'\f';
}

static bool is_digit(proven_u8 c) {
    return c >= (proven_u8)'0' && c <= (proven_u8)'9';
}

/* The parse failed at the very end of what we have: over a stream, that means "ask again
 * when more has arrived", not "this input is wrong". */
static void scan_mark_needs_more(proven_scan_t *scan) {
    if (scan) scan->needs_more = true;
}

static bool scan_valid(const proven_scan_t *scan) {
    return scan && (scan->view.size == 0 || scan->view.ptr != (void*)0) && scan->cursor <= scan->view.size;
}

static double proven_scan_double_from_bits(proven_u64 bits) {
    double value = 0.0;
    proven_sys_mem_copy(&value, &bits, sizeof value);
    return value;
}

void proven_scan_skip_whitespace(proven_scan_t *scan) {
    if (!scan_valid(scan)) return;
    while (scan->cursor < scan->view.size && is_whitespace(scan->view.ptr[scan->cursor])) {
        scan->cursor++;
    }
}

proven_result_u64_t proven_scan_u64(proven_scan_t *scan) {
    if (!scan_valid(scan)) return (proven_result_u64_t){ .err = PROVEN_ERR_INVALID_ARG };
    scan->needs_more = false;
    proven_scan_skip_whitespace(scan);

    proven_size_t start_cursor = scan->cursor;
    if (scan->cursor >= scan->view.size) {
        scan_mark_needs_more(scan);
        return (proven_result_u64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    if (!is_digit(scan->view.ptr[scan->cursor])) {
        return (proven_result_u64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }

    proven_u64 val = 0;
    while (scan->cursor < scan->view.size && is_digit(scan->view.ptr[scan->cursor])) {
        proven_u64 digit = (proven_u64)(scan->view.ptr[scan->cursor] - (proven_u8)'0');
        // Pre-multiplication overflow check
        if (val > (0xFFFFFFFFFFFFFFFFull / 10)) {
            scan->cursor = start_cursor;
            return (proven_result_u64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        val *= 10;
        // Post-addition overflow check
        if (val > (0xFFFFFFFFFFFFFFFFull - digit)) {
            scan->cursor = start_cursor;
            return (proven_result_u64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        val += digit;
        scan->cursor++;
    }

    return (proven_result_u64_t){ .val = val, .err = PROVEN_OK };
}

proven_result_i64_t proven_scan_i64(proven_scan_t *scan) {
    if (!scan_valid(scan)) return (proven_result_i64_t){ .err = PROVEN_ERR_INVALID_ARG };
    scan->needs_more = false;
    proven_scan_skip_whitespace(scan);

    proven_size_t start_cursor = scan->cursor;
    if (scan->cursor >= scan->view.size) {
        scan_mark_needs_more(scan);
        return (proven_result_i64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }

    bool negative = false;
    if (scan->view.ptr[scan->cursor] == (proven_u8)'-') {
        negative = true;
        scan->cursor++;
    } else if (scan->view.ptr[scan->cursor] == (proven_u8)'+') {
        scan->cursor++;
    }

    // No whitespace allowed after sign
    if (scan->cursor >= scan->view.size) {
        /* A sign and nothing after it. Over a stream the digits are simply still in
         * flight - "-" arrived, "12" has not - and calling that a malformed number
         * meant the buffered scanner could not read a number split across a read. */
        scan_mark_needs_more(scan);
        scan->cursor = start_cursor;
        return (proven_result_i64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    if (!is_digit(scan->view.ptr[scan->cursor])) {
        scan->cursor = start_cursor;
        return (proven_result_i64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }

    // We don't call proven_scan_u64 here because it skips whitespace.
    // Instead we do a manual unsigned parse.
    proven_u64 uval = 0;
    while (scan->cursor < scan->view.size && is_digit(scan->view.ptr[scan->cursor])) {
        proven_u64 digit = (proven_u64)(scan->view.ptr[scan->cursor] - (proven_u8)'0');
        if (uval > (0xFFFFFFFFFFFFFFFFull / 10)) {
            scan->cursor = start_cursor;
            return (proven_result_i64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        uval *= 10;
        if (uval > (0xFFFFFFFFFFFFFFFFull - digit)) {
            scan->cursor = start_cursor;
            return (proven_result_i64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        uval += digit;
        scan->cursor++;
    }

    if (negative) {
        if (uval > 0x8000000000000000ull) {
            scan->cursor = start_cursor;
            return (proven_result_i64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        proven_i64 final_val;
        if (uval == 0x8000000000000000ull) {
            final_val = (proven_i64)(-9223372036854775807ll - 1ll);
        } else {
            final_val = -(proven_i64)uval;
        }
        return (proven_result_i64_t){ .val = final_val, .err = PROVEN_OK };
    } else {
        if (uval > 0x7FFFFFFFFFFFFFFFull) {
            scan->cursor = start_cursor;
            return (proven_result_i64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        return (proven_result_i64_t){ .val = (proven_i64)uval, .err = PROVEN_OK };
    }
}

static int hex_value(proven_u8 c) {
    if (c >= (proven_u8)'0' && c <= (proven_u8)'9') return c - (proven_u8)'0';
    if (c >= (proven_u8)'a' && c <= (proven_u8)'f') return c - (proven_u8)'a' + 10;
    if (c >= (proven_u8)'A' && c <= (proven_u8)'F') return c - (proven_u8)'A' + 10;
    return -1;
}

/*
 * Hexadecimal digits at the cursor, with an optional "0x" / "0X" before them - the prefix is
 * taken only when a hex digit follows it, as strtoul takes it, so "0xg" is the number 0 followed
 * by "xg". On failure the cursor goes back to `start`.
 */
static proven_result_u64_t scan_hex_magnitude(proven_scan_t *scan, proven_size_t start) {
    const proven_u8 *p = scan->view.ptr;
    proven_size_t n = scan->view.size;
    if (scan->cursor >= n) {
        scan_mark_needs_more(scan);
        scan->cursor = start;
        return (proven_result_u64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    if (p[scan->cursor] == (proven_u8)'0' && scan->cursor + 1 < n &&
        (p[scan->cursor + 1] == (proven_u8)'x' || p[scan->cursor + 1] == (proven_u8)'X')) {
        if (scan->cursor + 2 < n && hex_value(p[scan->cursor + 2]) >= 0) {
            scan->cursor += 2;
        } else if (scan->cursor + 2 == n) {
            /* "0x" at the end of what has arrived: over a stream the digits may be next.
             * Parse the 0 as a complete view would, and ask for more. */
            scan_mark_needs_more(scan);
        }
    }
    if (hex_value(p[scan->cursor]) < 0) {
        scan->cursor = start;
        return (proven_result_u64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    proven_u64 val = 0;
    while (scan->cursor < n) {
        int d = hex_value(p[scan->cursor]);
        if (d < 0) break;
        if (val > (0xFFFFFFFFFFFFFFFFull >> 4)) {
            scan->cursor = start;
            return (proven_result_u64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        val = (val << 4) | (proven_u64)d;
        scan->cursor++;
    }
    return (proven_result_u64_t){ .val = val, .err = PROVEN_OK };
}

proven_result_u64_t proven_scan_u64_hex(proven_scan_t *scan) {
    if (!scan_valid(scan)) return (proven_result_u64_t){ .err = PROVEN_ERR_INVALID_ARG };
    scan->needs_more = false;
    proven_scan_skip_whitespace(scan);
    return scan_hex_magnitude(scan, scan->cursor);
}

proven_result_i64_t proven_scan_i64_hex(proven_scan_t *scan) {
    if (!scan_valid(scan)) return (proven_result_i64_t){ .err = PROVEN_ERR_INVALID_ARG };
    scan->needs_more = false;
    proven_scan_skip_whitespace(scan);

    proven_size_t start = scan->cursor;
    bool negative = false;
    if (scan->cursor < scan->view.size &&
        (scan->view.ptr[scan->cursor] == (proven_u8)'-' || scan->view.ptr[scan->cursor] == (proven_u8)'+')) {
        negative = scan->view.ptr[scan->cursor] == (proven_u8)'-';
        scan->cursor++;
    }
    proven_result_u64_t m = scan_hex_magnitude(scan, start);
    if (!proven_is_ok(m.err)) return (proven_result_i64_t){ .err = m.err };
    if (negative) {
        if (m.val > 0x8000000000000000ull) {
            scan->cursor = start;
            return (proven_result_i64_t){ .err = PROVEN_ERR_OVERFLOW };
        }
        proven_i64 v = (m.val == 0x8000000000000000ull) ? (proven_i64)(-9223372036854775807ll - 1ll) : -(proven_i64)m.val;
        return (proven_result_i64_t){ .val = v, .err = PROVEN_OK };
    }
    if (m.val > 0x7FFFFFFFFFFFFFFFull) {
        scan->cursor = start;
        return (proven_result_i64_t){ .err = PROVEN_ERR_OVERFLOW };
    }
    return (proven_result_i64_t){ .val = (proven_i64)m.val, .err = PROVEN_OK };
}

proven_result_f64_t proven_scan_f64(proven_scan_t *scan) {
    if (!scan_valid(scan)) return (proven_result_f64_t){ .err = PROVEN_ERR_INVALID_ARG };
    scan->needs_more = false;
    proven_scan_skip_whitespace(scan);

    proven_size_t start_cursor = scan->cursor;
    if (scan->cursor >= scan->view.size) {
        scan_mark_needs_more(scan);
        return (proven_result_f64_t){ .err = PROVEN_ERR_INVALID_ARG };
    }

    proven_float_parse_result_t parsed = proven_float_parse_ascii_token(scan->view.ptr + scan->cursor, scan->view.size - scan->cursor);
    if (parsed.err != PROVEN_OK) {
        /*
         * The parse FAILED - but over a stream that can mean "the float is not here" OR "only
         * the front of the float made it into the buffer". A boundary that leaves just "-",
         * "-3.", or "-3.2e" in the buffer parses as a failure, and without saying so the
         * buffered scanner would drop those bytes and desync every later scan (a real bug the
         * fmt->file->scanner round-trip caught). So if everything still in the view is a float
         * PREFIX - only sign/digit/point/exponent characters, nothing that definitively ends a
         * number - flag that more input might complete it. "abc" contains a non-float char and
         * is left as the genuine error it is.
         */
        bool all_float_chars = (scan->cursor < scan->view.size);
        for (proven_size_t i = scan->cursor; i < scan->view.size; ++i) {
            proven_u8 ch = scan->view.ptr[i];
            bool ok = (ch >= (proven_u8)'0' && ch <= (proven_u8)'9') ||
                      ch == (proven_u8)'.' || ch == (proven_u8)'+' || ch == (proven_u8)'-' ||
                      ch == (proven_u8)'e' || ch == (proven_u8)'E';
            if (!ok) { all_float_chars = false; break; }
        }
        if (all_float_chars) scan_mark_needs_more(scan);
        scan->cursor = start_cursor;
        return (proven_result_f64_t){ .err = parsed.err };
    }
    scan->cursor += parsed.consumed;

    if (parsed.kind == PROVEN_FLOAT_PARSE_KIND_INF) {
        proven_u64 bits = parsed.negative ? 0xfff0000000000000ull : 0x7ff0000000000000ull;
        return (proven_result_f64_t){ .val = proven_scan_double_from_bits(bits), .err = PROVEN_OK };
    }
    if (parsed.kind == PROVEN_FLOAT_PARSE_KIND_NAN) {
        proven_u64 bits = parsed.negative ? 0xfff8000000000000ull : 0x7ff8000000000000ull;
        return (proven_result_f64_t){ .val = proven_scan_double_from_bits(bits), .err = PROVEN_OK };
    }
    {
        double result = 0.0;
        proven_err_t err = proven_float_convert_decimal(scan->view.ptr + start_cursor, parsed.consumed, &result);
        if (err != PROVEN_OK) {
            scan->cursor = start_cursor;
            return (proven_result_f64_t){ .err = err };
        }

        /*
         * A float can be cut short by a buffer boundary, and unlike a truncated integer the
         * cut is invisible: the parser stops at a dangling exponent 'e' and returns the
         * MANTISSA as a perfectly valid float, silently dropping the "e-222" that had not
         * arrived yet. Over a complete view that is correct (a trailing 'e' is not part of
         * the number). Over a stream it is a truncated value committed as a success, and the
         * leftover "e-222" then desyncs every later scan.
         *
         * So flag "this float might continue if more input arrives" - and only then, so a
         * genuinely-complete "3.14energy" is not mistaken for an unfinished exponent:
         *   - the float ran to the end of the view (more digits/./e could follow), OR
         *   - it stopped at an 'e'/'E' whose exponent could still complete: 'e' at the view
         *     end, or 'e' then a lone sign at the view end. 'e' followed by a non-sign,
         *     non-digit char is a definitively-finished float, not a split exponent.
         */
        proven_size_t c = scan->cursor;
        proven_size_t n = scan->view.size;
        bool boundary_continuable = (c == n);
        if (!boundary_continuable && c < n) {
            proven_u8 ch = scan->view.ptr[c];
            if (ch == (proven_u8)'e' || ch == (proven_u8)'E') {
                if (c + 1 == n) boundary_continuable = true;
                else if (c + 2 == n) {
                    proven_u8 nx = scan->view.ptr[c + 1];
                    if (nx == (proven_u8)'+' || nx == (proven_u8)'-') boundary_continuable = true;
                }
            }
        }
        if (boundary_continuable) scan_mark_needs_more(scan);

        return (proven_result_f64_t){ .val = result, .err = PROVEN_OK };
    }
}

proven_result_u8str_view_t proven_scan_str(proven_scan_t *scan) {
    if (!scan_valid(scan)) return (proven_result_u8str_view_t){ .err = PROVEN_ERR_INVALID_ARG };
    scan->needs_more = false;
    proven_scan_skip_whitespace(scan);

    if (scan->cursor >= scan->view.size) {
        scan_mark_needs_more(scan);
        return (proven_result_u8str_view_t){ .err = PROVEN_ERR_INVALID_ARG };
    }

    proven_size_t start = scan->cursor;
    while (scan->cursor < scan->view.size && !is_whitespace(scan->view.ptr[scan->cursor])) {
        scan->cursor++;
    }

    proven_u8str_view_t token = {
        .ptr = scan->view.ptr + start,
        .size = scan->cursor - start
    };

    return (proven_result_u8str_view_t){ .val = token, .err = PROVEN_OK };
}

proven_err_t proven_scan_skip_until(proven_scan_t *scan, proven_u8str_view_t target) {
    if (!scan_valid(scan)) return PROVEN_ERR_INVALID_ARG;
    if (target.size > 0 && !target.ptr) return PROVEN_ERR_INVALID_ARG;
    if (target.size == 0) return PROVEN_OK;
    
    if (target.size > scan->view.size - scan->cursor) {
        return PROVEN_ERR_NOT_FOUND;
    }

    proven_size_t last = scan->view.size - target.size;
    for (proven_size_t i = scan->cursor; i <= last; ++i) {
        bool match = true;
        for (proven_size_t j = 0; j < target.size; ++j) {
            if (scan->view.ptr[i + j] != target.ptr[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            scan->cursor = i;
            return PROVEN_OK;
        }
    }

    return PROVEN_ERR_NOT_FOUND;
}

void proven_scan_skip_until_number(proven_scan_t *scan) {
    if (!scan_valid(scan)) return;
    while (scan->cursor < scan->view.size) {
        proven_u8 c = scan->view.ptr[scan->cursor];
        
        if (is_digit(c)) {
            break;
        }
        
        // Check for minus/plus sign directly followed by a digit
        if ((c == (proven_u8)'-' || c == (proven_u8)'+') && scan->cursor + 1 < scan->view.size) {
            if (is_digit(scan->view.ptr[scan->cursor + 1])) {
                break;
            }
        }
        
        scan->cursor++;
    }
}

/* The placeholder at p (which is '{'): "{}" or, for an integer, "{:x}" / "{:X}" - read it as
 * hexadecimal. Returns its length, or 0 when it is neither. */
static int placeholder_len(const char *p, bool *hex) {
    *hex = false;
    if (p[1] == '}') return 2;
    if (p[1] == ':' && (p[2] == 'x' || p[2] == 'X') && p[3] == '}') {
        *hex = true;
        return 4;
    }
    return 0;
}

static proven_result_i64_t scan_i64_as(proven_scan_t *scan, bool hex) {
    return hex ? proven_scan_i64_hex(scan) : proven_scan_i64(scan);
}

static proven_result_u64_t scan_u64_as(proven_scan_t *scan, bool hex) {
    return hex ? proven_scan_u64_hex(scan) : proven_scan_u64(scan);
}

static proven_err_t proven_scan_fmt_count_placeholders(const char *fmt, proven_size_t *out_count) {
    proven_size_t count = 0;

    for (const char *p = fmt; *p; ++p) {
        if (*p == '{') {
            bool hex;
            int len = placeholder_len(p, &hex);
            if (len == 0) {
                return PROVEN_ERR_INVALID_ARG;
            }
            if (count == PROVEN_SIZE_MAX) {
                return PROVEN_ERR_OVERFLOW;
            }
            count++;
            p += len - 1;
        }
    }

    *out_count = count;
    return PROVEN_OK;
}

proven_err_t proven_scan_fmt_internal(proven_scan_t *scan, const char *fmt, const proven_scan_arg_t *args, proven_size_t args_count) {
    if (!scan_valid(scan) || !fmt || (args_count > 0 && !args)) return PROVEN_ERR_INVALID_ARG;
    if (args_count == 0 || args[0].type != PROVEN_SCAN_ARG_TYPE_NONE) return PROVEN_ERR_INVALID_ARG;
    scan->needs_more = false;

    proven_size_t placeholder_count = 0;
    proven_err_t count_err = proven_scan_fmt_count_placeholders(fmt, &placeholder_count);

    if (count_err != PROVEN_OK) {
        return count_err;
    }

    // args_count is always at least 1 (the sentinel)
    if (placeholder_count != args_count - 1) {
        return PROVEN_ERR_INVALID_ARG;
    }

    proven_size_t arg_idx = 1; // start from 1 since index 0 is proven_scan_arg_none()
    const char *p = fmt;

    while (*p != '\0') {
        if (*p == '{') {
            bool hex = false;
            int ph_len = placeholder_len(p, &hex);
            if (ph_len > 0) {
                p += ph_len;
                
                if (arg_idx >= args_count) return PROVEN_ERR_INVALID_ARG;
                
                const proven_scan_arg_t *arg = &args[arg_idx++];
                
                switch (arg->type) {
                    case PROVEN_SCAN_ARG_TYPE_I32: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.i32) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_i64_t res = scan_i64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val < -2147483648ll || res.val > 2147483647ll) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.i32 = (proven_i32)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_U32: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.u32) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_u64_t res = scan_u64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val > 0xFFFFFFFFull) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.u32 = (proven_u32)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_I64: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.i64) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_i64_t res = scan_i64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        *arg->ptr.i64 = res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_U64: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.u64) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_u64_t res = scan_u64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        *arg->ptr.u64 = res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_SHORT: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.s) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_i64_t res = scan_i64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val < SHRT_MIN || res.val > SHRT_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.s = (short)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_USHORT: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.us) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_u64_t res = scan_u64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val > USHRT_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.us = (unsigned short)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_INT: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.i) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_i64_t res = scan_i64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val < INT_MIN || res.val > INT_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.i = (int)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_UINT: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.ui) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_u64_t res = scan_u64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val > UINT_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.ui = (unsigned int)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_LONG: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.l) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_i64_t res = scan_i64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val < LONG_MIN || res.val > LONG_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.l = (long)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_ULONG: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.ul) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_u64_t res = scan_u64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val > ULONG_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.ul = (unsigned long)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_LLONG: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.ll) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_i64_t res = scan_i64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val < LLONG_MIN || res.val > LLONG_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.ll = (long long)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_ULLONG: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.ull) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        proven_result_u64_t res = scan_u64_as(scan, hex);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        if (res.val > ULLONG_MAX) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_OVERFLOW;
                        }
                        *arg->ptr.ull = (unsigned long long)res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_F64: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.f64) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        if (hex) return PROVEN_ERR_INVALID_FORMAT;   /* {:x} names an integer */
                        proven_result_f64_t res = proven_scan_f64(scan);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        *arg->ptr.f64 = res.val;
                        break;
                    }
                    case PROVEN_SCAN_ARG_TYPE_STR_VIEW: {
                        proven_size_t arg_start = scan->cursor;
                        if (!arg->ptr.str_view) {
                            scan->cursor = arg_start;
                            return PROVEN_ERR_INVALID_ARG;
                        }
                        if (hex) return PROVEN_ERR_INVALID_FORMAT;
                        proven_result_u8str_view_t res = proven_scan_str(scan);
                        if (res.err != PROVEN_OK) {
                            scan->cursor = arg_start;
                            return res.err;
                        }
                        *arg->ptr.str_view = res.val;
                        break;
                    }
                    default:
                        return PROVEN_ERR_INVALID_ARG;
                }
            } else {
                /* "{}" and "{:x}" only; anything else was refused by the count above. */
                return PROVEN_ERR_INVALID_ARG;
            }
        } else {
            // Literal character match
            // skip whitespaces in scanner if format specifies a space
            if (is_whitespace((proven_u8)*p)) {
                proven_scan_skip_whitespace(scan);
                while (*p != '\0' && is_whitespace((proven_u8)*p)) p++;
                continue;
            } else {
                if (scan->cursor >= scan->view.size) {
                    /* The literal is not absent - it has not arrived. "key=" against a
                     * pipe that has so far delivered "ke" is not a mismatch. */
                    scan_mark_needs_more(scan);
                    return PROVEN_ERR_NOT_FOUND;
                }
                if (scan->view.ptr[scan->cursor] != (proven_u8)*p) {
                    return PROVEN_ERR_NOT_FOUND;
                }
                scan->cursor++;
                p++;
            }
        }
    }
    
    return PROVEN_OK;
}
