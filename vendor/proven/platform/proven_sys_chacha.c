#include "proven_sys_chacha.h"

/* ChaCha20 across vector lanes. Vector j holds word j of four (SSE2) or eight (AVX2) consecutive
 * blocks, so a quarter round is the same additions, XORs and rotations as the portable code, done
 * on every block at once; a transpose at the end turns the words back into each block's 64
 * bytes. No table and no data-dependent branch, as in the portable code. */

#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__)) && !defined(PROVEN_SYS_CHACHA_DISABLE)

#include <immintrin.h>

#define SSE_TARGET __attribute__((target("sse2")))
#define AVX_TARGET __attribute__((target("avx2")))

int proven_sys_chacha_width(void) {
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) return 8;
    if (__builtin_cpu_supports("sse2")) return 4;
    return 0;
}

/* Zero what held keystream, with the barrier proven_mem_wipe uses. */
static void cc_forget(void *p, size_t n) {
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; ++i) b[i] = 0;
    __asm__ __volatile__("" : : "r"(p) : "memory");
}

/* ---- four blocks, SSE2 ---- */

/* SSE2 has no rotation: a shift each way, except 16, which is a swap of the 16-bit halves. */
#define R4_16(x) _mm_shufflehi_epi16(_mm_shufflelo_epi16((x), 0xb1), 0xb1)
#define R4(x, n) _mm_or_si128(_mm_slli_epi32((x), (n)), _mm_srli_epi32((x), 32 - (n)))

#define QR4(a, b, c, d)                                                        \
    do {                                                                       \
        a = _mm_add_epi32(a, b); d = _mm_xor_si128(d, a); d = R4_16(d);        \
        c = _mm_add_epi32(c, d); b = _mm_xor_si128(b, c); b = R4(b, 12);       \
        a = _mm_add_epi32(a, b); d = _mm_xor_si128(d, a); d = R4(d, 8);        \
        c = _mm_add_epi32(c, d); b = _mm_xor_si128(b, c); b = R4(b, 7);        \
    } while (0)

/* Words j..j+3 of four blocks to each block's sixteen bytes of them, in place. */
#define T4(v0, v1, v2, v3)                                                     \
    do {                                                                       \
        __m128i t0 = _mm_unpacklo_epi32(v0, v1), t1 = _mm_unpacklo_epi32(v2, v3); \
        __m128i t2 = _mm_unpackhi_epi32(v0, v1), t3 = _mm_unpackhi_epi32(v2, v3); \
        v0 = _mm_unpacklo_epi64(t0, t1); v1 = _mm_unpackhi_epi64(t0, t1);       \
        v2 = _mm_unpacklo_epi64(t2, t3); v3 = _mm_unpackhi_epi64(t2, t3);       \
    } while (0)

SSE_TARGET static void cc_xor4(uint32_t ctr, const uint32_t s[16], const uint8_t *in, uint8_t *out) {
    __m128i in_[16], x[16];
    for (int j = 0; j < 16; ++j) in_[j] = _mm_set1_epi32((int)s[j]);
    in_[12] = _mm_add_epi32(_mm_set1_epi32((int)ctr), _mm_set_epi32(3, 2, 1, 0));
    for (int j = 0; j < 16; ++j) x[j] = in_[j];
    for (int i = 0; i < 10; ++i) {
        QR4(x[0], x[4], x[8], x[12]); QR4(x[1], x[5], x[9], x[13]);
        QR4(x[2], x[6], x[10], x[14]); QR4(x[3], x[7], x[11], x[15]);
        QR4(x[0], x[5], x[10], x[15]); QR4(x[1], x[6], x[11], x[12]);
        QR4(x[2], x[7], x[8], x[13]); QR4(x[3], x[4], x[9], x[14]);
    }
    for (int j = 0; j < 16; ++j) x[j] = _mm_add_epi32(x[j], in_[j]);
    T4(x[0], x[1], x[2], x[3]); T4(x[4], x[5], x[6], x[7]);
    T4(x[8], x[9], x[10], x[11]); T4(x[12], x[13], x[14], x[15]);
    /* After the transposes x[4g + k] is bytes 16g..16g+15 of block k. */
    for (int k = 0; k < 4; ++k)
        for (int g = 0; g < 4; ++g) {
            const __m128i *src = (const __m128i *)(in + 64 * k + 16 * g);
            _mm_storeu_si128((__m128i *)(out + 64 * k + 16 * g), _mm_xor_si128(_mm_loadu_si128(src), x[4 * g + k]));
        }
    cc_forget(x, sizeof x); cc_forget(in_, sizeof in_);
}

/* ---- eight blocks, AVX2 ---- */

#define R8(x, n) _mm256_or_si256(_mm256_slli_epi32((x), (n)), _mm256_srli_epi32((x), 32 - (n)))

#define QR8(a, b, c, d)                                                                        \
    do {                                                                                       \
        a = _mm256_add_epi32(a, b); d = _mm256_xor_si256(d, a); d = _mm256_shuffle_epi8(d, rot16); \
        c = _mm256_add_epi32(c, d); b = _mm256_xor_si256(b, c); b = R8(b, 12);                 \
        a = _mm256_add_epi32(a, b); d = _mm256_xor_si256(d, a); d = _mm256_shuffle_epi8(d, rot8); \
        c = _mm256_add_epi32(c, d); b = _mm256_xor_si256(b, c); b = R8(b, 7);                  \
    } while (0)

#define T8(v0, v1, v2, v3)                                                                     \
    do {                                                                                       \
        __m256i t0 = _mm256_unpacklo_epi32(v0, v1), t1 = _mm256_unpacklo_epi32(v2, v3);        \
        __m256i t2 = _mm256_unpackhi_epi32(v0, v1), t3 = _mm256_unpackhi_epi32(v2, v3);        \
        v0 = _mm256_unpacklo_epi64(t0, t1); v1 = _mm256_unpackhi_epi64(t0, t1);                \
        v2 = _mm256_unpacklo_epi64(t2, t3); v3 = _mm256_unpackhi_epi64(t2, t3);                \
    } while (0)

AVX_TARGET static void cc_xor8(uint32_t ctr, const uint32_t s[16], const uint8_t *in, uint8_t *out) {
    const __m256i rot16 = _mm256_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2,
                                          13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
    const __m256i rot8 = _mm256_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3,
                                         14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
    __m256i in_[16], x[16];
    for (int j = 0; j < 16; ++j) in_[j] = _mm256_set1_epi32((int)s[j]);
    in_[12] = _mm256_add_epi32(_mm256_set1_epi32((int)ctr), _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
    for (int j = 0; j < 16; ++j) x[j] = in_[j];
    for (int i = 0; i < 10; ++i) {
        QR8(x[0], x[4], x[8], x[12]); QR8(x[1], x[5], x[9], x[13]);
        QR8(x[2], x[6], x[10], x[14]); QR8(x[3], x[7], x[11], x[15]);
        QR8(x[0], x[5], x[10], x[15]); QR8(x[1], x[6], x[11], x[12]);
        QR8(x[2], x[7], x[8], x[13]); QR8(x[3], x[4], x[9], x[14]);
    }
    for (int j = 0; j < 16; ++j) x[j] = _mm256_add_epi32(x[j], in_[j]);
    T8(x[0], x[1], x[2], x[3]); T8(x[4], x[5], x[6], x[7]);
    T8(x[8], x[9], x[10], x[11]); T8(x[12], x[13], x[14], x[15]);
    /* Now x[4g + k] holds bytes 16g..16g+15 of block k in its low half and of block k + 4 in
     * its high half: two of them side by side are 32 bytes of one block. */
#define XOR32(at, v) _mm256_storeu_si256((__m256i *)(out + (at)), _mm256_xor_si256(_mm256_loadu_si256((const __m256i *)(in + (at))), (v)))
    for (int k = 0; k < 4; ++k) {
        size_t b = 64 * (size_t)k;
        XOR32(b, _mm256_permute2x128_si256(x[k], x[4 + k], 0x20));
        XOR32(b + 32, _mm256_permute2x128_si256(x[8 + k], x[12 + k], 0x20));
        XOR32(b + 256, _mm256_permute2x128_si256(x[k], x[4 + k], 0x31));
        XOR32(b + 288, _mm256_permute2x128_si256(x[8 + k], x[12 + k], 0x31));
    }
#undef XOR32
    cc_forget(x, sizeof x); cc_forget(in_, sizeof in_);
}

/* ---- Poly1305, four blocks at a time, AVX2 ---- */

/* Lane j of each vector holds one 26-bit limb of an accumulator that takes every fourth block.
 * Every group but the last: A = (A + M) * r^4. The last: A = (A + M) * r^(4-j) per lane, so
 * each block ends up multiplied by the power of r its position needs, and the four lanes are
 * summed. The 64-bit products of 26-bit limbs and their sums have room: limbs stay under 2^27,
 * five times r's under 2^29, five products under 2^58. */

static void p26_mul(uint32_t o[5], const uint32_t a[5], const uint32_t b[5]) {
    uint64_t s1 = b[1] * 5ull, s2 = b[2] * 5ull, s3 = b[3] * 5ull, s4 = b[4] * 5ull;
    uint64_t d0 = a[0] * (uint64_t)b[0] + a[1] * s4 + a[2] * s3 + a[3] * s2 + a[4] * s1;
    uint64_t d1 = a[0] * (uint64_t)b[1] + a[1] * (uint64_t)b[0] + a[2] * s4 + a[3] * s3 + a[4] * s2;
    uint64_t d2 = a[0] * (uint64_t)b[2] + a[1] * (uint64_t)b[1] + a[2] * (uint64_t)b[0] + a[3] * s4 + a[4] * s3;
    uint64_t d3 = a[0] * (uint64_t)b[3] + a[1] * (uint64_t)b[2] + a[2] * (uint64_t)b[1] + a[3] * (uint64_t)b[0] + a[4] * s4;
    uint64_t d4 = a[0] * (uint64_t)b[4] + a[1] * (uint64_t)b[3] + a[2] * (uint64_t)b[2] + a[3] * (uint64_t)b[1] + a[4] * (uint64_t)b[0];
    uint64_t c;
    c = d0 >> 26; d0 &= 0x3ffffff; d1 += c;
    c = d1 >> 26; d1 &= 0x3ffffff; d2 += c;
    c = d2 >> 26; d2 &= 0x3ffffff; d3 += c;
    c = d3 >> 26; d3 &= 0x3ffffff; d4 += c;
    c = d4 >> 26; d4 &= 0x3ffffff; d0 += c * 5;
    c = d0 >> 26; d0 &= 0x3ffffff; d1 += c;
    o[0] = (uint32_t)d0; o[1] = (uint32_t)d1; o[2] = (uint32_t)d2; o[3] = (uint32_t)d3; o[4] = (uint32_t)d4;
}

#define V64(x) _mm256_set1_epi64x((long long)(x))

/* d = h * r over the five limbs of each lane; s = 5 * r. Then one pass of carries. */
#define PMUL(h0, h1, h2, h3, h4, r0, r1, r2, r3, r4, s1, s2, s3, s4)                                    \
    do {                                                                                                \
        __m256i d0 = _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h0, r0), _mm256_mul_epu32(h1, s4)),  \
                     _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h2, s3), _mm256_mul_epu32(h3, s2)), _mm256_mul_epu32(h4, s1))); \
        __m256i d1 = _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h0, r1), _mm256_mul_epu32(h1, r0)),  \
                     _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h2, s4), _mm256_mul_epu32(h3, s3)), _mm256_mul_epu32(h4, s2))); \
        __m256i d2 = _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h0, r2), _mm256_mul_epu32(h1, r1)),  \
                     _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h2, r0), _mm256_mul_epu32(h3, s4)), _mm256_mul_epu32(h4, s3))); \
        __m256i d3 = _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h0, r3), _mm256_mul_epu32(h1, r2)),  \
                     _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h2, r1), _mm256_mul_epu32(h3, r0)), _mm256_mul_epu32(h4, s4))); \
        __m256i d4 = _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h0, r4), _mm256_mul_epu32(h1, r3)),  \
                     _mm256_add_epi64(_mm256_add_epi64(_mm256_mul_epu32(h2, r2), _mm256_mul_epu32(h3, r1)), _mm256_mul_epu32(h4, r0))); \
        __m256i c;                                                                                      \
        c = _mm256_srli_epi64(d0, 26); d0 = _mm256_and_si256(d0, m26); d1 = _mm256_add_epi64(d1, c);   \
        c = _mm256_srli_epi64(d3, 26); d3 = _mm256_and_si256(d3, m26); d4 = _mm256_add_epi64(d4, c);   \
        c = _mm256_srli_epi64(d1, 26); d1 = _mm256_and_si256(d1, m26); d2 = _mm256_add_epi64(d2, c);   \
        c = _mm256_srli_epi64(d4, 26); d4 = _mm256_and_si256(d4, m26);                                 \
        d0 = _mm256_add_epi64(d0, _mm256_add_epi64(c, _mm256_slli_epi64(c, 2)));                        \
        c = _mm256_srli_epi64(d2, 26); d2 = _mm256_and_si256(d2, m26); d3 = _mm256_add_epi64(d3, c);   \
        c = _mm256_srli_epi64(d0, 26); d0 = _mm256_and_si256(d0, m26); d1 = _mm256_add_epi64(d1, c);   \
        c = _mm256_srli_epi64(d3, 26); d3 = _mm256_and_si256(d3, m26); d4 = _mm256_add_epi64(d4, c);   \
        h0 = d0; h1 = d1; h2 = d2; h3 = d3; h4 = d4;                                                    \
    } while (0)

AVX_TARGET void proven_sys_poly1305_blocks(uint32_t h[5], const uint32_t r[5], const uint8_t *m, size_t blocks) {
    uint32_t p2[5], p3[5], p4[5];
    p26_mul(p2, r, r); p26_mul(p3, p2, r); p26_mul(p4, p2, p2);
    const __m256i m26 = V64(0x3ffffff), hibit = V64(1u << 24);
    const __m256i r0 = V64(p4[0]), r1 = V64(p4[1]), r2 = V64(p4[2]), r3 = V64(p4[3]), r4 = V64(p4[4]);
    const __m256i s1 = V64(p4[1] * 5ull), s2 = V64(p4[2] * 5ull), s3 = V64(p4[3] * 5ull), s4 = V64(p4[4] * 5ull);
    /* The state enters lane 0, the lane of the first block: both are multiplied by r^blocks. */
    __m256i h0 = _mm256_set_epi64x(0, 0, 0, h[0]), h1 = _mm256_set_epi64x(0, 0, 0, h[1]), h2 = _mm256_set_epi64x(0, 0, 0, h[2]);
    __m256i h3 = _mm256_set_epi64x(0, 0, 0, h[3]), h4 = _mm256_set_epi64x(0, 0, 0, h[4]);
    for (;;) {
        /* Blocks 0..3 as 64-bit halves: unpacking puts them in lanes 0, 2, 1, 3, and the final
         * powers below follow that order. */
        __m256i a = _mm256_loadu_si256((const __m256i *)m), b = _mm256_loadu_si256((const __m256i *)(m + 32));
        __m256i lo = _mm256_unpacklo_epi64(a, b), hi = _mm256_unpackhi_epi64(a, b);
        h0 = _mm256_add_epi64(h0, _mm256_and_si256(lo, m26));
        h1 = _mm256_add_epi64(h1, _mm256_and_si256(_mm256_srli_epi64(lo, 26), m26));
        h2 = _mm256_add_epi64(h2, _mm256_and_si256(_mm256_or_si256(_mm256_srli_epi64(lo, 52), _mm256_slli_epi64(hi, 12)), m26));
        h3 = _mm256_add_epi64(h3, _mm256_and_si256(_mm256_srli_epi64(hi, 14), m26));
        h4 = _mm256_add_epi64(h4, _mm256_or_si256(_mm256_srli_epi64(hi, 40), hibit));
        m += 64; blocks -= 4;
        if (blocks < 4) break;
        PMUL(h0, h1, h2, h3, h4, r0, r1, r2, r3, r4, s1, s2, s3, s4);
    }
    /* Lanes hold blocks 0, 2, 1, 3 of the last group: r^4, r^2, r^3, r^1. */
    const __m256i f0 = _mm256_set_epi64x(r[0], p3[0], p2[0], p4[0]), f1 = _mm256_set_epi64x(r[1], p3[1], p2[1], p4[1]);
    const __m256i f2 = _mm256_set_epi64x(r[2], p3[2], p2[2], p4[2]), f3 = _mm256_set_epi64x(r[3], p3[3], p2[3], p4[3]);
    const __m256i f4 = _mm256_set_epi64x(r[4], p3[4], p2[4], p4[4]);
    const __m256i g1 = _mm256_add_epi64(f1, _mm256_slli_epi64(f1, 2)), g2 = _mm256_add_epi64(f2, _mm256_slli_epi64(f2, 2));
    const __m256i g3 = _mm256_add_epi64(f3, _mm256_slli_epi64(f3, 2)), g4 = _mm256_add_epi64(f4, _mm256_slli_epi64(f4, 2));
    PMUL(h0, h1, h2, h3, h4, f0, f1, f2, f3, f4, g1, g2, g3, g4);
    uint64_t t[4], d[5];
    __m256i *hv[5] = { &h0, &h1, &h2, &h3, &h4 };
    for (int k = 0; k < 5; ++k) { _mm256_storeu_si256((__m256i *)t, *hv[k]); d[k] = t[0] + t[1] + t[2] + t[3]; }
    uint64_t c;
    c = d[0] >> 26; d[0] &= 0x3ffffff; d[1] += c;
    c = d[1] >> 26; d[1] &= 0x3ffffff; d[2] += c;
    c = d[2] >> 26; d[2] &= 0x3ffffff; d[3] += c;
    c = d[3] >> 26; d[3] &= 0x3ffffff; d[4] += c;
    c = d[4] >> 26; d[4] &= 0x3ffffff; d[0] += c * 5;
    c = d[0] >> 26; d[0] &= 0x3ffffff; d[1] += c;
    for (int k = 0; k < 5; ++k) h[k] = (uint32_t)d[k];
    cc_forget(p2, sizeof p2); cc_forget(p3, sizeof p3); cc_forget(p4, sizeof p4); cc_forget(t, sizeof t);
}

size_t proven_sys_chacha20_xor(const uint32_t state[16], const uint8_t *in, uint8_t *out, size_t len, int width) {
    uint32_t ctr = state[12];
    size_t done = 0;
    for (; width >= 8 && len - done >= 512; done += 512, ctr += 8) cc_xor8(ctr, state, in + done, out + done);
    for (; len - done >= 256; done += 256, ctr += 4) cc_xor4(ctr, state, in + done, out + done);
    /* Three or four blocks' worth left: four at once into a buffer cost less than three alone. */
    if (len - done > 128) {
        uint8_t ks[256] = { 0 };
        cc_xor4(ctr, state, ks, ks);
        for (size_t i = 0; i < len - done; ++i) out[done + i] = (uint8_t)(in[done + i] ^ ks[i]);
        cc_forget(ks, sizeof ks);
        done = len;
    }
    return done;
}

#else

int proven_sys_chacha_width(void) { return 0; }

size_t proven_sys_chacha20_xor(const uint32_t state[16], const uint8_t *in, uint8_t *out, size_t len, int width) {
    (void)state; (void)in; (void)out; (void)len; (void)width;
    return 0;
}

void proven_sys_poly1305_blocks(uint32_t h[5], const uint32_t r[5], const uint8_t *m, size_t blocks) {
    (void)h; (void)r; (void)m; (void)blocks;
}

#endif
