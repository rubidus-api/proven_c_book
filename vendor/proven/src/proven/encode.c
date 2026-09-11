#include "proven/encode.h"

/*
 * Hex and Base64 (RFC 4648). Pure computation: no allocation, no OS, no libc. The decoders
 * validate before they write, so malformed input from outside the program becomes a clean
 * PROVEN_ERR_INVALID_ENCODING rather than a read past the end or a silently short result.
 */

// -------------------------------------------------------------
// Hex
// -------------------------------------------------------------

static const char HEX_DIGITS[] = "0123456789abcdef";

/* -1 for a non-hex byte; 0..15 otherwise. Upper and lower case both decode. */
static int hex_value(proven_byte_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/*
 * The exact hex output size, or false when it does not fit in a proven_size_t.
 *
 * The public helper cannot return an error - it returns a size - so it answers
 * PROVEN_SIZE_MAX for a size that cannot be represented. That is a value no valid hex
 * output can have (hex output is always even, PROVEN_SIZE_MAX is odd), and it is not
 * zero, which is the honest answer for empty input and would be read as "fits anywhere".
 */
static bool hex_exact_size(proven_size_t n, proven_size_t *out) {
    return !PROVEN_CKD_MUL(out, n, (proven_size_t)2);
}

proven_size_t proven_hex_encoded_size(proven_size_t n) {
    proven_size_t need;
    return hex_exact_size(n, &need) ? need : PROVEN_SIZE_MAX;
}

proven_size_t proven_hex_decoded_size(proven_size_t n) {
    return n / 2;
}

proven_err_t proven_hex_encode(proven_mem_view_t data, proven_byte_t *out, proven_size_t out_cap,
                               proven_size_t *written_out) {
    if (written_out) *written_out = 0;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;

    /* The size is computed and judged BEFORE any byte is read or written. A wrapped
     * `need` is a small number: it passes this capacity check and the loop below then
     * writes past what the caller reserved. */
    proven_size_t need;
    if (!hex_exact_size(data.size, &need)) return PROVEN_ERR_OVERFLOW;
    if (need > out_cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    for (proven_size_t i = 0; i < data.size; ++i) {
        out[2 * i]     = (proven_byte_t)HEX_DIGITS[data.ptr[i] >> 4];
        out[2 * i + 1] = (proven_byte_t)HEX_DIGITS[data.ptr[i] & 0x0F];
    }
    if (written_out) *written_out = need;
    return PROVEN_OK;
}

proven_err_t proven_hex_decode(proven_mem_view_t text, proven_byte_t *out, proven_size_t out_cap,
                               proven_size_t *written_out) {
    if (written_out) *written_out = 0;
    if (text.size > 0 && !text.ptr) return PROVEN_ERR_INVALID_ARG;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;   /* the guard the encoders had and this lacked */

    if (text.size % 2 != 0) return PROVEN_ERR_INVALID_ENCODING;
    proven_size_t need = text.size / 2;
    if (need > out_cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    /* Validate the WHOLE input before writing a single byte, so a stray character near the end
     * does not leave a half-decoded prefix the caller might use. */
    for (proven_size_t i = 0; i < text.size; ++i) {
        if (hex_value(text.ptr[i]) < 0) return PROVEN_ERR_INVALID_ENCODING;
    }
    for (proven_size_t i = 0; i < need; ++i) {
        out[i] = (proven_byte_t)((hex_value(text.ptr[2 * i]) << 4) | hex_value(text.ptr[2 * i + 1]));
    }
    if (written_out) *written_out = need;
    return PROVEN_OK;
}

// -------------------------------------------------------------
// Base64
// -------------------------------------------------------------

static const char B64_STD[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char B64_URL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* -1 for a non-alphabet byte; 0..63 otherwise. Accepts BOTH alphabets, so the '+'/'/' of
 * standard Base64 and the '-'/'_' of the URL form decode through one table. */
static int b64_value(proven_byte_t c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

/*
 * The exact Base64 output size for `n` input bytes, or false when it does not fit.
 *
 * Four characters per whole three-byte group, then the tail: the padded form rounds the
 * tail up to four with '=', the unpadded form emits only the significant characters.
 * Written as (n / 3) * 4 plus the tail rather than ((n + 2) / 3) * 4, because the round-up
 * addition is itself where the old form wrapped.
 */
static bool base64_exact_size(proven_size_t n, bool pad, proven_size_t *out) {
    proven_size_t full = n / 3;
    proven_size_t rem = n % 3;
    proven_size_t need;
    if (PROVEN_CKD_MUL(&need, full, (proven_size_t)4)) return false;
    proven_size_t tail = 0;
    if (rem == 1) tail = pad ? 4 : 2;
    else if (rem == 2) tail = pad ? 4 : 3;
    if (PROVEN_CKD_ADD(&need, need, tail)) return false;
    *out = need;
    return true;
}

proven_size_t proven_base64_encoded_size(proven_size_t n) {
    /* The padded size, which is a safe upper bound for the unpadded form too. */
    proven_size_t need;
    return base64_exact_size(n, true, &need) ? need : PROVEN_SIZE_MAX;
}

proven_size_t proven_base64_decoded_size(proven_size_t n) {
    /* An UPPER BOUND, and it must hold for unpadded input too: proven_base64url_encode emits
     * no padding, so a text of length n%4 == 2 or 3 carries 1 or 2 real bytes that the floor
     * form (n/4)*3 dropped - which made the library unable to decode its own base64url output
     * into a buffer the caller sized with this function. Rounding n up to the next multiple of
     * 4 first covers both the padded and the unpadded tail.
     *
     * Rounding n up first is what the old form did, and n + 3 wrapped at the top of the
     * range. Counting whole groups and adding the tail separately reaches the same answer
     * without ever exceeding size_t: the largest value this can return is
     * (SIZE_MAX / 4) * 3 + 3, which fits. This bound therefore needs no overflow
     * sentinel - unlike the encoded sizes. */
    return (n / 4) * 3 + ((n % 4 != 0) ? 3 : 0);
}

static proven_err_t base64_encode_impl(proven_mem_view_t data, proven_byte_t *out,
                                       proven_size_t out_cap, proven_size_t *written_out,
                                       const char *alphabet, bool pad) {
    if (written_out) *written_out = 0;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;

    /* Exact output length, computed with checked arithmetic and judged before a single byte
     * is read or written. An unrepresentable size is refused outright, and that is a
     * different answer from "the buffer is too small": collapsing the two would let a
     * wrapped size be mistaken for a size that fits. */
    proven_size_t full = data.size / 3;
    proven_size_t rem = data.size % 3;   /* 0, 1, or 2 */
    proven_size_t need;
    if (!base64_exact_size(data.size, pad, &need)) return PROVEN_ERR_OVERFLOW;
    if (need > out_cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    proven_size_t o = 0;
    proven_size_t i = 0;
    /* Counted by whole groups. The old guard was `i + 3 <= data.size`, and that addition
     * is itself capable of wrapping at the top of the range. */
    for (proven_size_t g = 0; g < full; ++g, i += 3) {
        proven_u32 v = ((proven_u32)data.ptr[i] << 16) | ((proven_u32)data.ptr[i + 1] << 8) | data.ptr[i + 2];
        out[o++] = (proven_byte_t)alphabet[(v >> 18) & 0x3F];
        out[o++] = (proven_byte_t)alphabet[(v >> 12) & 0x3F];
        out[o++] = (proven_byte_t)alphabet[(v >> 6) & 0x3F];
        out[o++] = (proven_byte_t)alphabet[v & 0x3F];
    }
    if (rem == 1) {
        proven_u32 v = (proven_u32)data.ptr[i] << 16;
        out[o++] = (proven_byte_t)alphabet[(v >> 18) & 0x3F];
        out[o++] = (proven_byte_t)alphabet[(v >> 12) & 0x3F];
        if (pad) { out[o++] = '='; out[o++] = '='; }
    } else if (rem == 2) {
        proven_u32 v = ((proven_u32)data.ptr[i] << 16) | ((proven_u32)data.ptr[i + 1] << 8);
        out[o++] = (proven_byte_t)alphabet[(v >> 18) & 0x3F];
        out[o++] = (proven_byte_t)alphabet[(v >> 12) & 0x3F];
        out[o++] = (proven_byte_t)alphabet[(v >> 6) & 0x3F];
        if (pad) out[o++] = '=';
    }

    if (written_out) *written_out = o;
    return PROVEN_OK;
}

proven_err_t proven_base64_encode(proven_mem_view_t data, proven_byte_t *out, proven_size_t out_cap,
                                  proven_size_t *written_out) {
    return base64_encode_impl(data, out, out_cap, written_out, B64_STD, true);
}

proven_err_t proven_base64url_encode(proven_mem_view_t data, proven_byte_t *out, proven_size_t out_cap,
                                     proven_size_t *written_out) {
    return base64_encode_impl(data, out, out_cap, written_out, B64_URL, false);
}

proven_err_t proven_base64_decode(proven_mem_view_t text, proven_byte_t *out, proven_size_t out_cap,
                                  proven_size_t *written_out) {
    if (written_out) *written_out = 0;
    if (text.size > 0 && !text.ptr) return PROVEN_ERR_INVALID_ARG;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;   /* the guard the encoders had and this lacked */

    /* Count trailing padding, then validate everything. Padding may only be the last one or two
     * characters, and only on a padded (multiple-of-4) input. */
    proven_size_t n = text.size;
    proven_size_t pad = 0;
    while (n > 0 && text.ptr[n - 1] == '=') { ++pad; --n; }

    if (pad > 2) return PROVEN_ERR_INVALID_ENCODING;
    if (pad > 0 && text.size % 4 != 0) return PROVEN_ERR_INVALID_ENCODING;

    /* Every remaining character must be in the alphabet - no '=' in the middle, no stray byte,
     * no skipped whitespace. */
    for (proven_size_t i = 0; i < n; ++i) {
        if (b64_value(text.ptr[i]) < 0) return PROVEN_ERR_INVALID_ENCODING;
    }

    /* n significant characters carry n*6 bits. A leftover of exactly 1 character (6 bits)
     * cannot have come from any whole byte, so it is an impossible length. */
    proven_size_t rem = n % 4;
    if (rem == 1) return PROVEN_ERR_INVALID_ENCODING;

    proven_size_t out_len = (n / 4) * 3;
    if (rem == 2) out_len += 1;
    else if (rem == 3) out_len += 2;
    if (out_len > out_cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    proven_size_t o = 0;
    proven_size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        proven_u32 v = ((proven_u32)b64_value(text.ptr[i]) << 18)
                     | ((proven_u32)b64_value(text.ptr[i + 1]) << 12)
                     | ((proven_u32)b64_value(text.ptr[i + 2]) << 6)
                     | ((proven_u32)b64_value(text.ptr[i + 3]));
        out[o++] = (proven_byte_t)(v >> 16);
        out[o++] = (proven_byte_t)(v >> 8);
        out[o++] = (proven_byte_t)v;
    }
    if (rem == 2) {
        proven_u32 v = ((proven_u32)b64_value(text.ptr[i]) << 18)
                     | ((proven_u32)b64_value(text.ptr[i + 1]) << 12);
        out[o++] = (proven_byte_t)(v >> 16);
    } else if (rem == 3) {
        proven_u32 v = ((proven_u32)b64_value(text.ptr[i]) << 18)
                     | ((proven_u32)b64_value(text.ptr[i + 1]) << 12)
                     | ((proven_u32)b64_value(text.ptr[i + 2]) << 6);
        out[o++] = (proven_byte_t)(v >> 16);
        out[o++] = (proven_byte_t)(v >> 8);
    }

    if (written_out) *written_out = o;
    return PROVEN_OK;
}
