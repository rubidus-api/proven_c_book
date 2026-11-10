#include "proven/hash_legacy.h"

/*
 * SHA-1 (FIPS 180-4) and MD5 (RFC 1321), written from those two documents.
 *
 * Both are the same construction: a 64-byte block, a compression function over a few 32-bit
 * words, and a final block that carries the message length in bits. They differ in the
 * compression function and in byte order - SHA-1 is big-endian throughout, MD5 little-endian -
 * so the block buffering is written once and the two digests supply the rest.
 */

static proven_u32 legacy_rotl(proven_u32 x, unsigned n) {
    return (x << n) | (x >> (32u - n));
}

// -----------------------------------------------------------------------------
// The shared block buffer
// -----------------------------------------------------------------------------

typedef void (*legacy_compress_fn)(proven_u32 *state, const proven_byte_t block[64]);

/* Top up a partly filled block first; then compress whole blocks straight from the input with
 * no copy; keep the tail. The same shape as proven_sha256_update. */
static void legacy_update(proven_u32 *state, proven_u64 *length, proven_byte_t block[64],
                          proven_size_t *block_len, legacy_compress_fn compress,
                          proven_mem_view_t data) {
    if (data.size == 0) return;   /* {NULL, 0}: nothing to add, and no arithmetic on NULL below */
    *length += data.size;
    const proven_byte_t *p = data.ptr;
    proven_size_t n = data.size;
    if (*block_len > 0) {
        proven_size_t take = 64u - *block_len;
        if (take > n) take = n;
        for (proven_size_t i = 0; i < take; ++i) block[*block_len + i] = p[i];
        *block_len += take;
        p += take;
        n -= take;
        if (*block_len < 64u) return;
        compress(state, block);
        *block_len = 0;
    }
    while (n >= 64u) {
        compress(state, p);
        p += 64;
        n -= 64;
    }
    for (proven_size_t i = 0; i < n; ++i) block[i] = p[i];
    *block_len = n;
}

/* Append 0x80, zeros up to byte 56 of a block, then the bit length in eight bytes - most
 * significant first for SHA-1, least significant first for MD5. When the 0x80 lands at or past
 * byte 56 there is no room for the length, so that block is compressed and the length goes in
 * a block of its own: the case the 56-byte vectors exist to catch. */
static void legacy_pad(proven_u32 *state, proven_u64 length, proven_byte_t block[64],
                       proven_size_t *block_len, legacy_compress_fn compress, bool big_endian) {
    proven_u64 bits = length * 8u;
    proven_size_t n = *block_len;
    block[n++] = 0x80;
    if (n > 56u) {
        while (n < 64u) block[n++] = 0;
        compress(state, block);
        n = 0;
    }
    while (n < 56u) block[n++] = 0;
    for (unsigned i = 0; i < 8u; ++i) {
        unsigned shift = big_endian ? 56u - 8u * i : 8u * i;
        block[56u + i] = (proven_byte_t)(bits >> shift);
    }
    compress(state, block);
    *block_len = 0;
}

static void legacy_to_hex(const proven_byte_t *digest, proven_size_t n, char *out) {
    static const char d[] = "0123456789abcdef";
    for (proven_size_t i = 0; i < n; ++i) {
        out[i * 2]     = d[digest[i] >> 4];
        out[i * 2 + 1] = d[digest[i] & 0xf];
    }
    out[n * 2] = 0;
}

// -----------------------------------------------------------------------------
// SHA-1
// -----------------------------------------------------------------------------

static void sha1_compress(proven_u32 *state, const proven_byte_t block[64]) {
    proven_u32 w[80];
    for (unsigned i = 0; i < 16u; ++i) {
        w[i] = ((proven_u32)block[i * 4] << 24) | ((proven_u32)block[i * 4 + 1] << 16) |
               ((proven_u32)block[i * 4 + 2] << 8) | (proven_u32)block[i * 4 + 3];
    }
    for (unsigned i = 16; i < 80u; ++i) {
        w[i] = legacy_rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    proven_u32 a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (unsigned i = 0; i < 80u; ++i) {
        proven_u32 f, k;
        if (i < 20u)      { f = (b & c) | (~b & d);          k = 0x5a827999u; }
        else if (i < 40u) { f = b ^ c ^ d;                   k = 0x6ed9eba1u; }
        else if (i < 60u) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
        else              { f = b ^ c ^ d;                   k = 0xca62c1d6u; }
        proven_u32 t = legacy_rotl(a, 5) + f + e + k + w[i];
        e = d; d = c; c = legacy_rotl(b, 30); b = a; a = t;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

/* The block functions by themselves, for the legacy TLS unit: its constant-time record MAC
 * runs them over blocks it assembles under masks. Declared in proven_internal_crypto.h. */
void proven_sha1_compress(proven_u32 state[5], const proven_byte_t block[64]) { sha1_compress(state, block); }

void proven_sha1_init(proven_sha1_t *ctx) {
    if (!ctx) return;
    ctx->state[0] = 0x67452301u; ctx->state[1] = 0xefcdab89u;
    ctx->state[2] = 0x98badcfeu; ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xc3d2e1f0u;
    ctx->length = 0;
    ctx->block_len = 0;
}

void proven_sha1_update(proven_sha1_t *ctx, proven_mem_view_t data) {
    if (!ctx || (data.size > 0 && !data.ptr)) return;
    legacy_update(ctx->state, &ctx->length, ctx->block, &ctx->block_len, sha1_compress, data);
}

void proven_sha1_final(proven_sha1_t *ctx, proven_byte_t out[PROVEN_SHA1_SIZE]) {
    if (!ctx || !out) return;
    legacy_pad(ctx->state, ctx->length, ctx->block, &ctx->block_len, sha1_compress, true);
    for (unsigned i = 0; i < 5u; ++i) {
        out[i * 4]     = (proven_byte_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (proven_byte_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (proven_byte_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (proven_byte_t)(ctx->state[i]);
    }
}

void proven_sha1(proven_mem_view_t data, proven_byte_t out[PROVEN_SHA1_SIZE]) {
    proven_sha1_t ctx;
    proven_sha1_init(&ctx);
    proven_sha1_update(&ctx, data);
    proven_sha1_final(&ctx, out);
}

void proven_sha1_to_hex(const proven_byte_t digest[PROVEN_SHA1_SIZE], char out[41]) {
    if (!digest || !out) return;
    legacy_to_hex(digest, PROVEN_SHA1_SIZE, out);
}

// -----------------------------------------------------------------------------
// MD5
// -----------------------------------------------------------------------------

/* RFC 1321 section 3.4: T[i] = floor(2^32 * abs(sin(i + 1))), and the per-round rotations. */
static const proven_u32 md5_t[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};

static const proven_u8 md5_s[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

static void md5_compress(proven_u32 *state, const proven_byte_t block[64]) {
    proven_u32 x[16];
    for (unsigned i = 0; i < 16u; ++i) {
        x[i] = (proven_u32)block[i * 4] | ((proven_u32)block[i * 4 + 1] << 8) |
               ((proven_u32)block[i * 4 + 2] << 16) | ((proven_u32)block[i * 4 + 3] << 24);
    }

    proven_u32 a = state[0], b = state[1], c = state[2], d = state[3];
    for (unsigned i = 0; i < 64u; ++i) {
        proven_u32 f;
        unsigned g;
        if (i < 16u)      { f = (b & c) | (~b & d); g = i; }
        else if (i < 32u) { f = (d & b) | (~d & c); g = (5u * i + 1u) & 15u; }
        else if (i < 48u) { f = b ^ c ^ d;          g = (3u * i + 5u) & 15u; }
        else              { f = c ^ (b | ~d);       g = (7u * i) & 15u; }
        proven_u32 t = a + f + md5_t[i] + x[g];
        a = d; d = c; c = b;
        b += legacy_rotl(t, md5_s[i]);
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

void proven_md5_compress(proven_u32 state[4], const proven_byte_t block[64]) { md5_compress(state, block); }

void proven_md5_init(proven_md5_t *ctx) {
    if (!ctx) return;
    ctx->state[0] = 0x67452301u; ctx->state[1] = 0xefcdab89u;
    ctx->state[2] = 0x98badcfeu; ctx->state[3] = 0x10325476u;
    ctx->length = 0;
    ctx->block_len = 0;
}

void proven_md5_update(proven_md5_t *ctx, proven_mem_view_t data) {
    if (!ctx || (data.size > 0 && !data.ptr)) return;
    legacy_update(ctx->state, &ctx->length, ctx->block, &ctx->block_len, md5_compress, data);
}

void proven_md5_final(proven_md5_t *ctx, proven_byte_t out[PROVEN_MD5_SIZE]) {
    if (!ctx || !out) return;
    legacy_pad(ctx->state, ctx->length, ctx->block, &ctx->block_len, md5_compress, false);
    for (unsigned i = 0; i < 4u; ++i) {
        out[i * 4]     = (proven_byte_t)(ctx->state[i]);
        out[i * 4 + 1] = (proven_byte_t)(ctx->state[i] >> 8);
        out[i * 4 + 2] = (proven_byte_t)(ctx->state[i] >> 16);
        out[i * 4 + 3] = (proven_byte_t)(ctx->state[i] >> 24);
    }
}

void proven_md5(proven_mem_view_t data, proven_byte_t out[PROVEN_MD5_SIZE]) {
    proven_md5_t ctx;
    proven_md5_init(&ctx);
    proven_md5_update(&ctx, data);
    proven_md5_final(&ctx, out);
}

void proven_md5_to_hex(const proven_byte_t digest[PROVEN_MD5_SIZE], char out[33]) {
    if (!digest || !out) return;
    legacy_to_hex(digest, PROVEN_MD5_SIZE, out);
}
