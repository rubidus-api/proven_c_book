#include "proven/utf.h"
#include "proven/align.h"
#include "proven_internal_memrange.h"

/*
 * UTF-8 <-> UTF-16, strict (RFC 3629, Unicode 15 chapter 3 table 3-7). Pure computation.
 *
 * Both decoders answer in three ways, and the difference between the last two is the whole
 * reason this file exists rather than a call to MultiByteToWideChar: that function fails the
 * whole buffer for a character cut in half, so text read in pieces cannot be converted with it.
 *
 *   > 0  a whole character of that many units was decoded;
 *     0  the input ends inside a character that could still be valid - NEED_MORE;
 *    -1  the input is malformed, and more of it would not help.
 *
 * The UTF-8 decoder checks every byte it can see before it asks for more, so "E0 80" is
 * malformed at once (E0 must be followed by A0..BF) rather than reported as incomplete.
 */

static int utf8_decode_span(const proven_byte_t *s, proven_size_t n, proven_u32 *cp_out,
                            proven_size_t *bad_span) {
    proven_byte_t b0 = s[0];
    *bad_span = 1;
    if (b0 < 0x80) {
        *cp_out = b0;
        return 1;
    }

    int len;
    proven_byte_t lo = 0x80, hi = 0xBF;
    proven_u32 cp;
    if (b0 < 0xC2) {
        return -1;                       /* a continuation byte, or an overlong C0/C1 lead */
    } else if (b0 < 0xE0) {
        len = 2; cp = b0 & 0x1Fu;
    } else if (b0 < 0xF0) {
        len = 3; cp = b0 & 0x0Fu;
        if (b0 == 0xE0) lo = 0xA0;       /* overlong below U+0800 */
        if (b0 == 0xED) hi = 0x9F;       /* U+D800..U+DFFF are not characters */
    } else if (b0 < 0xF5) {
        len = 4; cp = b0 & 0x07u;
        if (b0 == 0xF0) lo = 0x90;       /* overlong below U+10000 */
        if (b0 == 0xF4) hi = 0x8F;       /* above U+10FFFF */
    } else {
        return -1;
    }

    for (int i = 1; i < len; ++i) {
        if ((proven_size_t)i >= n) return 0;
        proven_byte_t b = s[i];
        proven_byte_t min = (i == 1) ? lo : 0x80;
        proven_byte_t max = (i == 1) ? hi : 0xBF;
        if (b < min || b > max) {
            *bad_span = (proven_size_t)i;    /* the maximal subpart: everything before this byte */
            return -1;
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }
    *cp_out = cp;
    return len;
}

static int utf8_decode(const proven_byte_t *s, proven_size_t n, proven_u32 *cp_out) {
    proven_size_t unused;
    return utf8_decode_span(s, n, cp_out, &unused);
}

proven_utf8_char_t proven_utf8_decode_next(proven_u8str_view_t s, proven_size_t pos) {
    proven_utf8_char_t out = { PROVEN_ERR_INVALID_ARG, 0, 0 };
    if (s.size > 0 && !s.ptr) return out;
    if (pos > s.size) { out.err = PROVEN_ERR_OUT_OF_BOUNDS; return out; }
    if (pos == s.size) { out.err = PROVEN_ERR_EOF; return out; }

    proven_size_t bad = 1;
    proven_u32 cp = 0;
    int r = utf8_decode_span(s.ptr + pos, s.size - pos, &cp, &bad);
    if (r > 0) {
        out.err = PROVEN_OK;
        out.cp = cp;
        out.len = (proven_size_t)r;
    } else if (r == 0) {
        out.err = PROVEN_ERR_NEED_MORE;   /* a valid start the view ends inside */
        out.len = s.size - pos;
    } else {
        out.err = PROVEN_ERR_INVALID_ENCODING;
        out.len = bad;
    }
    return out;
}

static int utf16_decode(const proven_u16 *s, proven_size_t n, proven_u32 *cp_out) {
    proven_u32 u = s[0];
    if (u < 0xD800u || u > 0xDFFFu) {
        *cp_out = u;
        return 1;
    }
    if (u >= 0xDC00u) return -1;         /* a low surrogate with no high one before it */
    if (n < 2) return 0;
    proven_u32 v = s[1];
    if (v < 0xDC00u || v > 0xDFFFu) return -1;
    *cp_out = 0x10000u + ((u - 0xD800u) << 10) + (v - 0xDC00u);
    return 2;
}

static int utf8_width(proven_u32 cp) {
    if (cp < 0x80u) return 1;
    if (cp < 0x800u) return 2;
    if (cp < 0x10000u) return 3;
    return 4;
}

static void utf8_put(proven_byte_t *out, proven_u32 cp, int width) {
    switch (width) {
    case 1:
        out[0] = (proven_byte_t)cp;
        break;
    case 2:
        out[0] = (proven_byte_t)(0xC0u | (cp >> 6));
        out[1] = (proven_byte_t)(0x80u | (cp & 0x3Fu));
        break;
    case 3:
        out[0] = (proven_byte_t)(0xE0u | (cp >> 12));
        out[1] = (proven_byte_t)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (proven_byte_t)(0x80u | (cp & 0x3Fu));
        break;
    default:
        out[0] = (proven_byte_t)(0xF0u | (cp >> 18));
        out[1] = (proven_byte_t)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (proven_byte_t)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (proven_byte_t)(0x80u | (cp & 0x3Fu));
        break;
    }
}

// -------------------------------------------------------------
// Partial
// -------------------------------------------------------------

proven_utf_step_t proven_utf8_to_utf16_partial(proven_u8str_view_t src, proven_u16 *out, proven_size_t out_cap) {
    proven_utf_step_t st = { PROVEN_OK, 0, 0 };
    if ((src.size > 0 && !src.ptr) || (out_cap > 0 && !out)) {
        st.err = PROVEN_ERR_INVALID_ARG;
        return st;
    }

    while (st.consumed < src.size) {
        proven_u32 cp;
        int len = utf8_decode(src.ptr + st.consumed, src.size - st.consumed, &cp);
        if (len < 0) { st.err = PROVEN_ERR_INVALID_ENCODING; return st; }
        if (len == 0) { st.err = PROVEN_ERR_NEED_MORE; return st; }

        proven_size_t units = (cp >= 0x10000u) ? 2 : 1;
        if (out_cap - st.written < units) { st.err = PROVEN_ERR_OUT_OF_BOUNDS; return st; }

        if (units == 1) {
            out[st.written] = (proven_u16)cp;
        } else {
            proven_u32 v = cp - 0x10000u;
            out[st.written]     = (proven_u16)(0xD800u + (v >> 10));
            out[st.written + 1] = (proven_u16)(0xDC00u + (v & 0x3FFu));
        }
        st.written += units;
        st.consumed += (proven_size_t)len;
    }
    return st;
}

proven_utf_step_t proven_utf16_to_utf8_partial(const proven_u16 *src, proven_size_t n,
                                               proven_byte_t *out, proven_size_t out_cap) {
    proven_utf_step_t st = { PROVEN_OK, 0, 0 };
    if ((n > 0 && !src) || (out_cap > 0 && !out)) {
        st.err = PROVEN_ERR_INVALID_ARG;
        return st;
    }

    while (st.consumed < n) {
        proven_u32 cp;
        int len = utf16_decode(src + st.consumed, n - st.consumed, &cp);
        if (len < 0) { st.err = PROVEN_ERR_INVALID_ENCODING; return st; }
        if (len == 0) { st.err = PROVEN_ERR_NEED_MORE; return st; }

        int width = utf8_width(cp);
        if (out_cap - st.written < (proven_size_t)width) { st.err = PROVEN_ERR_OUT_OF_BOUNDS; return st; }
        utf8_put(out + st.written, cp, width);
        st.written += (proven_size_t)width;
        st.consumed += (proven_size_t)len;
    }
    return st;
}

// -------------------------------------------------------------
// Measuring
// -------------------------------------------------------------

proven_result_size_t proven_utf8_to_utf16_size(proven_u8str_view_t src) {
    proven_result_size_t res = { PROVEN_OK, 0 };
    if (src.size > 0 && !src.ptr) { res.err = PROVEN_ERR_INVALID_ARG; return res; }

    /* No overflow check is needed: every UTF-16 unit comes from at least one input byte. */
    proven_size_t i = 0;
    while (i < src.size) {
        proven_u32 cp;
        int len = utf8_decode(src.ptr + i, src.size - i, &cp);
        if (len <= 0) { res.err = PROVEN_ERR_INVALID_ENCODING; res.value = 0; return res; }
        res.value += (cp >= 0x10000u) ? 2 : 1;
        i += (proven_size_t)len;
    }
    return res;
}

proven_result_size_t proven_utf16_to_utf8_size(const proven_u16 *src, proven_size_t n) {
    proven_result_size_t res = { PROVEN_OK, 0 };
    if (n > 0 && !src) { res.err = PROVEN_ERR_INVALID_ARG; return res; }

    proven_size_t i = 0;
    while (i < n) {
        proven_u32 cp;
        int len = utf16_decode(src + i, n - i, &cp);
        if (len <= 0) { res.err = PROVEN_ERR_INVALID_ENCODING; res.value = 0; return res; }
        /* Three bytes per unit at most, so this can wrap only for a unit count above
         * SIZE_MAX / 3 - which no real array reaches, and which is checked anyway. */
        if (PROVEN_CKD_ADD(&res.value, res.value, (proven_size_t)utf8_width(cp))) {
            res.err = PROVEN_ERR_OVERFLOW;
            res.value = 0;
            return res;
        }
        i += (proven_size_t)len;
    }
    return res;
}

// -------------------------------------------------------------
// Fixed capacity, atomic
// -------------------------------------------------------------

proven_err_t proven_utf8_to_utf16(proven_u8str_view_t src, proven_u16 *out, proven_size_t out_cap,
                                  proven_size_t *written) {
    if (written) *written = 0;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;

    proven_result_size_t need = proven_utf8_to_utf16_size(src);
    if (!proven_is_ok(need.err)) return need.err;
    if (need.value > out_cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    proven_utf_step_t st = proven_utf8_to_utf16_partial(src, out, out_cap);
    if (!proven_is_ok(st.err)) return st.err;   /* unreachable after the measure; kept honest */
    if (written) *written = st.written;
    return PROVEN_OK;
}

proven_err_t proven_utf16_to_utf8(const proven_u16 *src, proven_size_t n, proven_byte_t *out,
                                  proven_size_t out_cap, proven_size_t *written) {
    if (written) *written = 0;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;

    proven_result_size_t need = proven_utf16_to_utf8_size(src, n);
    if (!proven_is_ok(need.err)) return need.err;
    if (need.value > out_cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    proven_utf_step_t st = proven_utf16_to_utf8_partial(src, n, out, out_cap);
    if (!proven_is_ok(st.err)) return st.err;
    if (written) *written = st.written;
    return PROVEN_OK;
}

// -------------------------------------------------------------
// Growable, atomic
// -------------------------------------------------------------

/*
 * Growable, atomic. The exact output size is known after validation, so the string is grown
 * ONCE (doubling, as the strings' own _grow calls do) and the text is converted straight into
 * its storage. After the grow nothing can fail, so there is nothing to roll back: on any failure
 * the string has not been touched. (It used to convert through a 256-unit stack chunk and
 * append_grow each chunk - every byte copied twice, possibly several reallocations, and a
 * hand-written rollback for an allocation failing half-way; code review.)
 *
 * Input overlapping the string's storage is refused: the grow may move it.
 */
static proven_size_t grow_cap(proven_size_t cap, proven_size_t required, proven_size_t min_start) {
    proven_size_t c = cap ? cap : min_start;
    while (c < required) {
        if (PROVEN_CKD_MUL(&c, c, (proven_size_t)2)) return required;
    }
    return c;
}

proven_err_t proven_utf16_append_to_u8str(proven_allocator_t alloc, proven_u8str_t *dst,
                                          const proven_u16 *src, proven_size_t n) {
    if (!dst) return PROVEN_ERR_INVALID_ARG;
    if (n > 0 && !src) return PROVEN_ERR_INVALID_ARG;
    proven_size_t src_bytes;
    if (PROVEN_CKD_MUL(&src_bytes, n, sizeof(proven_u16))) return PROVEN_ERR_OVERFLOW;
    if (proven_range_overlaps(dst->internal.ptr, dst->internal.cap, src, src_bytes)) return PROVEN_ERR_INVALID_ARG;

    proven_result_size_t need = proven_utf16_to_utf8_size(src, n);
    if (!proven_is_ok(need.err)) return need.err;
    if (need.value == 0) return proven_u8str_append_grow(alloc, dst, (proven_u8str_view_t){ 0 });

    proven_size_t required;   /* old length + text + terminator */
    if (PROVEN_CKD_ADD(&required, dst->internal.len, need.value) || PROVEN_CKD_ADD(&required, required, 1)) {
        return PROVEN_ERR_OVERFLOW;
    }
    if (required > dst->internal.cap) {
        proven_size_t new_cap = grow_cap(dst->internal.cap, required, 16);
        proven_err_t e = proven_u8str_reserve(alloc, dst, new_cap);
        if (!proven_is_ok(e)) return e;
    }

    proven_byte_t *at = dst->internal.ptr + dst->internal.len;
    proven_utf_step_t st = proven_utf16_to_utf8_partial(src, n, at, need.value);
    if (st.err != PROVEN_OK || st.written != need.value) return PROVEN_ERR_INVALID_STATE;   /* validated above */
    dst->internal.len += need.value;
    dst->internal.ptr[dst->internal.len] = 0;
    return PROVEN_OK;
}

#ifndef PROVEN_NO_U16STR
proven_err_t proven_utf8_append_to_u16str(proven_allocator_t alloc, proven_u16str_t *dst,
                                          proven_u8str_view_t src) {
    if (!dst) return PROVEN_ERR_INVALID_ARG;
    if (src.size > 0 && !src.ptr) return PROVEN_ERR_INVALID_ARG;
    if (proven_range_overlaps(dst->internal.ptr, dst->internal.cap, src.ptr, src.size)) return PROVEN_ERR_INVALID_ARG;

    proven_result_size_t need = proven_utf8_to_utf16_size(src);
    if (!proven_is_ok(need.err)) return need.err;
    if (need.value == 0) return proven_u16str_append_grow(alloc, dst, (proven_u16str_view_t){ 0 });

    proven_size_t old_units = dst->internal.len / sizeof(proven_u16);
    proven_size_t required_units, required_bytes;   /* old + text + terminator */
    if (PROVEN_CKD_ADD(&required_units, old_units, need.value) || PROVEN_CKD_ADD(&required_units, required_units, 1) ||
        PROVEN_CKD_MUL(&required_bytes, required_units, sizeof(proven_u16))) {
        return PROVEN_ERR_OVERFLOW;
    }
    if (required_bytes > dst->internal.cap) {
        if (!proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
        proven_size_t new_cap = grow_cap(dst->internal.cap, required_bytes, 16 * sizeof(proven_u16));
        proven_result_mem_mut_t m = alloc.realloc_fn(alloc.ctx, dst->internal.ptr, dst->internal.cap, new_cap,
                                                     PROVEN_DEFAULT_ALIGNMENT);
        if (!proven_is_ok(m.err)) return m.err;
        dst->internal.ptr = (proven_byte_t *)m.value.ptr;
        dst->internal.cap = new_cap;
    }

    proven_u16 *units = (proven_u16 *)(void *)dst->internal.ptr;
    proven_utf_step_t st = proven_utf8_to_utf16_partial(src, units + old_units, need.value);
    if (st.err != PROVEN_OK || st.written != need.value) return PROVEN_ERR_INVALID_STATE;   /* validated above */
    dst->internal.len += need.value * sizeof(proven_u16);
    units[old_units + need.value] = 0;
    return PROVEN_OK;
}
#endif
