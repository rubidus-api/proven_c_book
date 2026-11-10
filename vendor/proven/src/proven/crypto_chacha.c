#include "proven_internal_crypto.h"
#ifndef PROVEN_FREESTANDING
#include "../../platform/proven_sys_chacha.h"
#endif

/* ChaCha20, Poly1305 and their AEAD (RFC 8439). Additions, rotations and XORs only: there is
 * no table and no data-dependent branch in any of it. */

static proven_u32 cc_load32(const proven_byte_t *p) {
    return (proven_u32)p[0] | ((proven_u32)p[1] << 8) | ((proven_u32)p[2] << 16) | ((proven_u32)p[3] << 24);
}

static void cc_store32(proven_byte_t *p, proven_u32 v) {
    p[0] = (proven_byte_t)v; p[1] = (proven_byte_t)(v >> 8); p[2] = (proven_byte_t)(v >> 16); p[3] = (proven_byte_t)(v >> 24);
}

static proven_u32 cc_rotl(proven_u32 x, int n) { return (x << n) | (x >> (32 - n)); }

#define CC_QR(a, b, c, d)                \
    do {                                 \
        a += b; d ^= a; d = cc_rotl(d, 16); \
        c += d; b ^= c; b = cc_rotl(b, 12); \
        a += b; d ^= a; d = cc_rotl(d, 8);  \
        c += d; b ^= c; b = cc_rotl(b, 7);  \
    } while (0)

static void cc_block(const proven_u32 in[16], proven_byte_t out[64]) {
    proven_u32 x[16];
    for (int i = 0; i < 16; ++i) x[i] = in[i];
    for (int i = 0; i < 10; ++i) {
        CC_QR(x[0], x[4], x[8], x[12]);
        CC_QR(x[1], x[5], x[9], x[13]);
        CC_QR(x[2], x[6], x[10], x[14]);
        CC_QR(x[3], x[7], x[11], x[15]);
        CC_QR(x[0], x[5], x[10], x[15]);
        CC_QR(x[1], x[6], x[11], x[12]);
        CC_QR(x[2], x[7], x[8], x[13]);
        CC_QR(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; ++i) cc_store32(out + 4 * i, x[i] + in[i]);
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x, .size = sizeof x });
}

static void cc_setup(proven_u32 st[16], const proven_byte_t key[32], proven_u32 counter, const proven_byte_t nonce[12]) {
    st[0] = 0x61707865u; st[1] = 0x3320646eu; st[2] = 0x79622d32u; st[3] = 0x6b206574u;
    for (int i = 0; i < 8; ++i) st[4 + i] = cc_load32(key + 4 * i);
    st[12] = counter;
    for (int i = 0; i < 3; ++i) st[13 + i] = cc_load32(nonce + 4 * i);
}

/* The vector path's width: what the processor has, or what a test asked for. Read once. */
#ifndef PROVEN_FREESTANDING
static int g_cc_width = -2;                      /* -2: not asked yet; -1: asked for the widest */

static int cc_width(void) {
    int w = __atomic_load_n(&g_cc_width, __ATOMIC_RELAXED);
    if (w < 0) {
        w = proven_sys_chacha_width();
        __atomic_store_n(&g_cc_width, w, __ATOMIC_RELAXED);
    }
    return w;
}
#endif

int proven_crypto_chacha_force_width(int width) {
#ifdef PROVEN_FREESTANDING
    (void)width;
    return 0;
#else
    int have = proven_sys_chacha_width();
    int w = width < 0 ? have : width >= 8 && have >= 8 ? 8 : width >= 4 && have >= 4 ? 4 : 0;
    __atomic_store_n(&g_cc_width, w, __ATOMIC_RELAXED);
    return w;
#endif
}

void proven_crypto_chacha20(const proven_byte_t key[32], proven_u32 counter, const proven_byte_t nonce[12],
                            const proven_byte_t *in, proven_byte_t *out, proven_size_t len) {
    proven_u32 st[16];
    proven_byte_t ks[64];
    cc_setup(st, key, counter, nonce);
#ifndef PROVEN_FREESTANDING
    /* Groups of blocks on the vector path; at most two blocks left, the portable way below. */
    int w = len > 128 ? cc_width() : 0;
    if (w > 0) {
        proven_size_t done = proven_sys_chacha20_xor(st, in, out, len, w);
        st[12] += (proven_u32)(done / 64);
        in += done; out += done; len -= done;
    }
#endif
    while (len > 0) {
        cc_block(st, ks);
        st[12]++;
        proven_size_t n = len < 64 ? len : 64;
        for (proven_size_t i = 0; i < n; ++i) out[i] = (proven_byte_t)(in[i] ^ ks[i]);
        in += n; out += n; len -= n;
    }
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)st, .size = sizeof st });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = ks, .size = sizeof ks });
}

#ifdef PROVEN_CRYPTO_POLY1305_64

/* Poly1305 over three limbs of 44, 44 and 42 bits: a product of a limb and r is at most 2^88,
 * five of the reduced ones summed fit 128 bits with room (poly1305-donna's 64-bit layout). */

__extension__ typedef unsigned __int128 poly_wide_t;

static proven_u64 cc_load64(const proven_byte_t *p) {
    return (proven_u64)cc_load32(p) | ((proven_u64)cc_load32(p + 4) << 32);
}

static void cc_store64(proven_byte_t *p, proven_u64 v) { cc_store32(p, (proven_u32)v); cc_store32(p + 4, (proven_u32)(v >> 32)); }

#define M44 0xfffffffffffull
#define M42 0x3ffffffffffull

void proven_crypto_poly1305_init(proven_crypto_poly1305_t *st, const proven_byte_t key[32]) {
    proven_u64 t0 = cc_load64(key), t1 = cc_load64(key + 8);
    st->r[0] = t0 & 0xffc0fffffffull;
    st->r[1] = ((t0 >> 44) | (t1 << 20)) & 0xfffffc0ffffull;
    st->r[2] = (t1 >> 24) & 0x00ffffffc0full;
    for (int i = 0; i < 3; ++i) st->h[i] = 0;
    st->pad[0] = cc_load64(key + 16); st->pad[1] = cc_load64(key + 24);
    st->buf_len = 0;
}

static void poly_blocks(proven_crypto_poly1305_t *st, const proven_byte_t *m, proven_size_t blocks, proven_u64 hibit) {
    const proven_u64 r0 = st->r[0], r1 = st->r[1], r2 = st->r[2];
    const proven_u64 s1 = r1 * (5 << 2), s2 = r2 * (5 << 2);
    proven_u64 h0 = st->h[0], h1 = st->h[1], h2 = st->h[2];
    while (blocks-- > 0) {
        proven_u64 t0 = cc_load64(m), t1 = cc_load64(m + 8);
        h0 += t0 & M44;
        h1 += ((t0 >> 44) | (t1 << 20)) & M44;
        h2 += ((t1 >> 24) & M42) | hibit;
        poly_wide_t d0 = (poly_wide_t)h0 * r0 + (poly_wide_t)h1 * s2 + (poly_wide_t)h2 * s1;
        poly_wide_t d1 = (poly_wide_t)h0 * r1 + (poly_wide_t)h1 * r0 + (poly_wide_t)h2 * s2;
        poly_wide_t d2 = (poly_wide_t)h0 * r2 + (poly_wide_t)h1 * r1 + (poly_wide_t)h2 * r0;
        proven_u64 c;
        c = (proven_u64)(d0 >> 44); h0 = (proven_u64)d0 & M44; d1 += c;
        c = (proven_u64)(d1 >> 44); h1 = (proven_u64)d1 & M44; d2 += c;
        c = (proven_u64)(d2 >> 42); h2 = (proven_u64)d2 & M42;
        h0 += c * 5; c = h0 >> 44; h0 &= M44;
        h1 += c;
        m += 16;
    }
    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2;
}

#define POLY_HIBIT (1ull << 40)

#else

/* Poly1305 over five 26-bit limbs: every product fits 64 bits with room for the sums. */

void proven_crypto_poly1305_init(proven_crypto_poly1305_t *st, const proven_byte_t key[32]) {
    st->r[0] = cc_load32(key + 0) & 0x3ffffffu;
    st->r[1] = (cc_load32(key + 3) >> 2) & 0x3ffff03u;
    st->r[2] = (cc_load32(key + 6) >> 4) & 0x3ffc0ffu;
    st->r[3] = (cc_load32(key + 9) >> 6) & 0x3f03fffu;
    st->r[4] = (cc_load32(key + 12) >> 8) & 0x00fffffu;
    for (int i = 0; i < 5; ++i) st->h[i] = 0;
    for (int i = 0; i < 4; ++i) st->pad[i] = cc_load32(key + 16 + 4 * i);
    st->buf_len = 0;
}

static void poly_blocks(proven_crypto_poly1305_t *st, const proven_byte_t *m, proven_size_t blocks, proven_u32 hibit) {
    const proven_u32 r0 = st->r[0], r1 = st->r[1], r2 = st->r[2], r3 = st->r[3], r4 = st->r[4];
    const proven_u32 s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    proven_u32 h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3], h4 = st->h[4];
    while (blocks-- > 0) {
        h0 += cc_load32(m + 0) & 0x3ffffffu;
        h1 += (cc_load32(m + 3) >> 2) & 0x3ffffffu;
        h2 += (cc_load32(m + 6) >> 4) & 0x3ffffffu;
        h3 += (cc_load32(m + 9) >> 6) & 0x3ffffffu;
        h4 += (cc_load32(m + 12) >> 8) | hibit;
        proven_u64 d0 = (proven_u64)h0 * r0 + (proven_u64)h1 * s4 + (proven_u64)h2 * s3 + (proven_u64)h3 * s2 + (proven_u64)h4 * s1;
        proven_u64 d1 = (proven_u64)h0 * r1 + (proven_u64)h1 * r0 + (proven_u64)h2 * s4 + (proven_u64)h3 * s3 + (proven_u64)h4 * s2;
        proven_u64 d2 = (proven_u64)h0 * r2 + (proven_u64)h1 * r1 + (proven_u64)h2 * r0 + (proven_u64)h3 * s4 + (proven_u64)h4 * s3;
        proven_u64 d3 = (proven_u64)h0 * r3 + (proven_u64)h1 * r2 + (proven_u64)h2 * r1 + (proven_u64)h3 * r0 + (proven_u64)h4 * s4;
        proven_u64 d4 = (proven_u64)h0 * r4 + (proven_u64)h1 * r3 + (proven_u64)h2 * r2 + (proven_u64)h3 * r1 + (proven_u64)h4 * r0;
        proven_u64 c;
        c = d0 >> 26; h0 = (proven_u32)d0 & 0x3ffffffu; d1 += c;
        c = d1 >> 26; h1 = (proven_u32)d1 & 0x3ffffffu; d2 += c;
        c = d2 >> 26; h2 = (proven_u32)d2 & 0x3ffffffu; d3 += c;
        c = d3 >> 26; h3 = (proven_u32)d3 & 0x3ffffffu; d4 += c;
        c = d4 >> 26; h4 = (proven_u32)d4 & 0x3ffffffu;
        h0 += (proven_u32)c * 5;
        h1 += h0 >> 26; h0 &= 0x3ffffffu;
        m += 16;
    }
    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2; st->h[3] = h3; st->h[4] = h4;
}

#define POLY_HIBIT (1u << 24)

#endif

/* Long runs of whole blocks on the AVX2 path, four lanes at a time; it works on five 26-bit
 * limbs, so the 64-bit layout is carried there and back. Returns the blocks it did. */
static proven_size_t poly_vector(proven_crypto_poly1305_t *st, const proven_byte_t *m, proven_size_t blocks) {
#ifdef PROVEN_FREESTANDING
    (void)st; (void)m; (void)blocks;
    return 0;
#else
    if (blocks < 16 || cc_width() < 8) return 0;
    blocks &= ~(proven_size_t)3;
#ifdef PROVEN_CRYPTO_POLY1305_64
    proven_u64 h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], c;
    c = h0 >> 44; h0 &= M44; h1 += c;
    c = h1 >> 44; h1 &= M44; h2 += c;
    c = h2 >> 42; h2 &= M42; h0 += c * 5;
    c = h0 >> 44; h0 &= M44; h1 += c;
    c = h1 >> 44; h1 &= M44; h2 += c;
    const proven_u64 r0 = st->r[0], r1 = st->r[1], r2 = st->r[2];
    proven_u32 h[5] = { (proven_u32)(h0 & 0x3ffffff), (proven_u32)(((h0 >> 26) | (h1 << 18)) & 0x3ffffff), (proven_u32)((h1 >> 8) & 0x3ffffff),
                        (proven_u32)(((h1 >> 34) | (h2 << 10)) & 0x3ffffff), (proven_u32)(h2 >> 16) };
    proven_u32 r[5] = { (proven_u32)(r0 & 0x3ffffff), (proven_u32)(((r0 >> 26) | (r1 << 18)) & 0x3ffffff), (proven_u32)((r1 >> 8) & 0x3ffffff),
                        (proven_u32)(((r1 >> 34) | (r2 << 10)) & 0x3ffffff), (proven_u32)(r2 >> 16) };
    proven_sys_poly1305_blocks(h, r, m, blocks);
    /* Back to 44, 44 and 42 bits, the carries kept: the limbs may be a little over 26 bits. */
    h0 = (proven_u64)h[0] + ((proven_u64)h[1] << 26);
    h1 = (h0 >> 44) + ((proven_u64)h[2] << 8) + ((proven_u64)h[3] << 34); h0 &= M44;
    h2 = (h1 >> 44) + ((proven_u64)h[4] << 16); h1 &= M44;
    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2;
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)h, .size = sizeof h });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)r, .size = sizeof r });
#else
    proven_sys_poly1305_blocks(st->h, st->r, m, blocks);
#endif
    return blocks;
#endif
}

void proven_crypto_poly1305_update(proven_crypto_poly1305_t *st, const proven_byte_t *m, proven_size_t len) {
    if (st->buf_len > 0) {
        while (len > 0 && st->buf_len < 16) { st->buf[st->buf_len++] = *m++; len--; }
        if (st->buf_len < 16) return;
        poly_blocks(st, st->buf, 1, POLY_HIBIT);
        st->buf_len = 0;
    }
    proven_size_t blocks = len / 16;
    proven_size_t fast = poly_vector(st, m, blocks);
    m += fast * 16; len -= fast * 16; blocks -= fast;
    if (blocks > 0) { poly_blocks(st, m, blocks, POLY_HIBIT); m += blocks * 16; len -= blocks * 16; }
    while (len > 0) { st->buf[st->buf_len++] = *m++; len--; }
}

void proven_crypto_poly1305_final(proven_crypto_poly1305_t *st, proven_byte_t tag[16]) {
    if (st->buf_len > 0) {
        /* A short last block is closed with a 1 byte and zeros, and has no 2^128 bit. */
        st->buf[st->buf_len++] = 1;
        while (st->buf_len < 16) st->buf[st->buf_len++] = 0;
        poly_blocks(st, st->buf, 1, 0);
    }
#ifdef PROVEN_CRYPTO_POLY1305_64
    proven_u64 h0 = st->h[0], h1 = st->h[1], h2 = st->h[2];
    proven_u64 c;
    c = h1 >> 44; h1 &= M44; h2 += c;
    c = h2 >> 42; h2 &= M42; h0 += c * 5;
    c = h0 >> 44; h0 &= M44; h1 += c;
    c = h1 >> 44; h1 &= M44; h2 += c;
    c = h2 >> 42; h2 &= M42; h0 += c * 5;
    c = h0 >> 44; h0 &= M44; h1 += c;
    /* g = h + 5 - 2^130, kept when it did not borrow; a mask, not a branch (h is SECRET). */
    proven_u64 g0 = h0 + 5; c = g0 >> 44; g0 &= M44;
    proven_u64 g1 = h1 + c; c = g1 >> 44; g1 &= M44;
    proven_u64 g2 = h2 + c - (1ull << 42);
    proven_u64 mask = (g2 >> 63) - 1u;           /* all ones when g2 did not go negative */
    g0 &= mask; g1 &= mask; g2 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0; h1 = (h1 & mask) | g1; h2 = (h2 & mask) | g2;
    /* h + pad mod 2^128. */
    proven_u64 t0 = st->pad[0], t1 = st->pad[1];
    h0 += t0 & M44; c = h0 >> 44; h0 &= M44;
    h1 += (((t0 >> 44) | (t1 << 20)) & M44) + c; c = h1 >> 44; h1 &= M44;
    h2 += ((t1 >> 24) & M42) + c; h2 &= M42;
    cc_store64(tag, h0 | (h1 << 44));
    cc_store64(tag + 8, (h1 >> 20) | (h2 << 24));
#else
    proven_u32 h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3], h4 = st->h[4];
    proven_u32 c;
    c = h1 >> 26; h1 &= 0x3ffffffu; h2 += c;
    c = h2 >> 26; h2 &= 0x3ffffffu; h3 += c;
    c = h3 >> 26; h3 &= 0x3ffffffu; h4 += c;
    c = h4 >> 26; h4 &= 0x3ffffffu; h0 += c * 5;
    c = h0 >> 26; h0 &= 0x3ffffffu; h1 += c;
    /* g = h + 5 - 2^130. If that did not borrow, h was at least p and g is the reduced value.
     * The choice is made with a mask, not a branch: h is SECRET until the pad is added. */
    proven_u32 g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffffu;
    proven_u32 g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffffu;
    proven_u32 g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffffu;
    proven_u32 g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffffu;
    proven_u32 g4 = h4 + c - (1u << 26);
    proven_u32 mask = (g4 >> 31) - 1u;          /* all ones when g4 did not go negative */
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0; h1 = (h1 & mask) | g1; h2 = (h2 & mask) | g2; h3 = (h3 & mask) | g3; h4 = (h4 & mask) | g4;
    h0 = h0 | (h1 << 26);
    h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 << 8);
    proven_u64 f;
    f = (proven_u64)h0 + st->pad[0]; h0 = (proven_u32)f;
    f = (proven_u64)h1 + st->pad[1] + (f >> 32); h1 = (proven_u32)f;
    f = (proven_u64)h2 + st->pad[2] + (f >> 32); h2 = (proven_u32)f;
    f = (proven_u64)h3 + st->pad[3] + (f >> 32); h3 = (proven_u32)f;
    cc_store32(tag + 0, h0); cc_store32(tag + 4, h1); cc_store32(tag + 8, h2); cc_store32(tag + 12, h3);
#endif
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)st, .size = sizeof *st });
}

static void cc_aead_tag(const proven_byte_t key[32], const proven_byte_t nonce[12], proven_mem_view_t aad,
                        const proven_byte_t *cipher, proven_size_t cipher_len, proven_byte_t tag[16]) {
    static const proven_byte_t zeros[16] = { 0 };
    proven_byte_t poly_key[64];
    for (int i = 0; i < 64; ++i) poly_key[i] = 0;
    proven_crypto_chacha20(key, 0, nonce, poly_key, poly_key, 64);     /* block 0 keys the MAC */
    proven_crypto_poly1305_t st;
    proven_crypto_poly1305_init(&st, poly_key);
    proven_mem_wipe((proven_mem_mut_t){ .ptr = poly_key, .size = sizeof poly_key });
    if (aad.size > 0) proven_crypto_poly1305_update(&st, aad.ptr, aad.size);
    if (aad.size % 16 != 0) proven_crypto_poly1305_update(&st, zeros, 16 - aad.size % 16);
    if (cipher_len > 0) proven_crypto_poly1305_update(&st, cipher, cipher_len);
    if (cipher_len % 16 != 0) proven_crypto_poly1305_update(&st, zeros, 16 - cipher_len % 16);
    proven_byte_t lens[16];
    proven_u64 a = (proven_u64)aad.size, c = (proven_u64)cipher_len;
    for (int i = 0; i < 8; ++i) { lens[i] = (proven_byte_t)(a >> (8 * i)); lens[8 + i] = (proven_byte_t)(c >> (8 * i)); }
    proven_crypto_poly1305_update(&st, lens, 16);
    proven_crypto_poly1305_final(&st, tag);
}

void proven_crypto_chacha20poly1305_seal(const proven_byte_t key[32], const proven_byte_t nonce[12],
                                         proven_mem_view_t aad, proven_mem_view_t plain,
                                         proven_byte_t *out, proven_byte_t tag[16]) {
    if (plain.size > 0) proven_crypto_chacha20(key, 1, nonce, plain.ptr, out, plain.size);
    cc_aead_tag(key, nonce, aad, out, plain.size, tag);
}

bool proven_crypto_chacha20poly1305_open(const proven_byte_t key[32], const proven_byte_t nonce[12],
                                         proven_mem_view_t aad, proven_mem_view_t cipher,
                                         const proven_byte_t tag[16], proven_byte_t *out) {
    proven_byte_t want[16];
    cc_aead_tag(key, nonce, aad, cipher.ptr, cipher.size, want);
    bool ok = proven_mem_equal_ct((proven_mem_view_t){ .ptr = want, .size = 16 }, (proven_mem_view_t){ .ptr = tag, .size = 16 });
    PROVEN_CT_PUBLIC(&ok, sizeof ok);            /* whether the tag matched is what the caller is told */
    if (!ok) {
        for (proven_size_t i = 0; i < cipher.size; ++i) out[i] = 0;
        return false;
    }
    if (cipher.size > 0) proven_crypto_chacha20(key, 1, nonce, cipher.ptr, out, cipher.size);
    return true;
}
