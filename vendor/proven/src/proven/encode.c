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

proven_size_t proven_hex_encoded_size(proven_size_t n) {
    return n * 2;
}

proven_size_t proven_hex_decoded_size(proven_size_t n) {
    return n / 2;
}

proven_err_t proven_hex_encode(proven_mem_view_t data, proven_byte_t *out, proven_size_t out_cap,
                               proven_size_t *written_out) {
    if (written_out) *written_out = 0;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;

    proven_size_t need = data.size * 2;
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

proven_size_t proven_base64_encoded_size(proven_size_t n) {
    return ((n + 2) / 3) * 4;
}

proven_size_t proven_base64_decoded_size(proven_size_t n) {
    /* An UPPER BOUND, and it must hold for unpadded input too: proven_base64url_encode emits
     * no padding, so a text of length n%4 == 2 or 3 carries 1 or 2 real bytes that the floor
     * form (n/4)*3 dropped - which made the library unable to decode its own base64url output
     * into a buffer the caller sized with this function. Rounding n up to the next multiple of
     * 4 first covers both the padded and the unpadded tail. */
    return ((n + 3) / 4) * 3;
}

static proven_err_t base64_encode_impl(proven_mem_view_t data, proven_byte_t *out,
                                       proven_size_t out_cap, proven_size_t *written_out,
                                       const char *alphabet, bool pad) {
    if (written_out) *written_out = 0;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    if (out_cap > 0 && !out) return PROVEN_ERR_INVALID_ARG;

    /* Exact output length: 4 chars per full 3-byte group, then the tail. The padded form rounds
     * the tail up to 4 with '='; the unpadded form emits only the significant characters. */
    proven_size_t full = data.size / 3;
    proven_size_t rem = data.size % 3;   /* 0, 1, or 2 */
    proven_size_t need = full * 4;
    if (rem == 1) need += pad ? 4 : 2;
    else if (rem == 2) need += pad ? 4 : 3;
    if (need > out_cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    proven_size_t o = 0;
    proven_size_t i = 0;
    for (; i + 3 <= data.size; i += 3) {
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
