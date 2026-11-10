#include "proven_internal_crypto.h"
#include "proven/hash.h"

/* Curve25519: the field, X25519 and Ed25519.
 *
 * A field element is kept one of two ways, by the same choice as the limb of the
 * multi-precision arithmetic (PROVEN_CRYPTO_MP_LIMB_BITS):
 *
 *  - five limbs of 51 bits, where a product of two is the compiler's 128-bit integer: a
 *    multiplication is 25 machine multiplications, a squaring 15;
 *  - ten limbs of alternately 26 and 25 bits (radix 2^25.5), so that every product of two
 *    limbs, and the sum of the ten such products in a column, fits a 64-bit word on every
 *    target.
 *
 * Either way the limbs are unsigned, subtraction adds a multiple of p first, and what a
 * function gives back is "carried": every limb within its width, but for the second, which may
 * be a few above it. Nothing here branches on, or indexes by, a field element or a scalar -
 * but for signature verification's functions, named `_vartime`, which see only public values.
 * Everything below the field is written once, over fe_t and FE_LIMBS.
 *
 * A multiple of the base point - a public key, the R of a signature - is taken from a table
 * of sixteen fixed points by a signed comb of five teeth, as P-256's generator is in
 * crypto_ec.c. */

/* The group order L = 2^252 + 27742317777372353535851937790883648493, big-endian. */
static const proven_byte_t ED_L[32] = {
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0xde, 0xf9, 0xde, 0xa2, 0xf7, 0x9c, 0xd6, 0x58, 0x12, 0x63, 0x1a, 0x5c, 0xf5, 0xd3, 0xed };

#if PROVEN_CRYPTO_MP_LIMB_BITS == 64

/* ---- the field on five limbs of 51 bits ---- */

typedef proven_u64 fe_limb_t;
#define FE_LIMBS 5
typedef fe_limb_t fe_t[FE_LIMBS];
__extension__ typedef unsigned __int128 fe_wide_t;
#define FE_M51 0x7ffffffffffffull

static const fe_t FE_D = { 0x34dca135978a3ull, 0x1a8283b156ebdull, 0x5e7a26001c029ull, 0x739c663a03cbbull, 0x52036cee2b6ffull };
static const fe_t FE_D2 = { 0x69b9426b2f159ull, 0x35050762add7aull, 0x3cf44c0038052ull, 0x6738cc7407977ull, 0x2406d9dc56dffull };
static const fe_t FE_SQRTM1 = { 0x61b274a0ea0b0ull, 0x0d5a5fc8f189dull, 0x7ef5e9cbd0c60ull, 0x78595a6804c9eull, 0x2b8324804fc1dull };
static const fe_t ED_BX = { 0x62d608f25d51aull, 0x412a4b4f6592aull, 0x75b7171a4b31dull, 0x1ff60527118feull, 0x216936d3cd6e5ull };
static const fe_t ED_BY = { 0x6666666666658ull, 0x4ccccccccccccull, 0x1999999999999ull, 0x3333333333333ull, 0x6666666666666ull };

static void fe_carry(fe_t h) {
    proven_u64 c;
    for (int i = 0; i < 4; ++i) { c = h[i] >> 51; h[i] &= FE_M51; h[i + 1] += c; }
    c = h[4] >> 51; h[4] &= FE_M51; h[0] += 19 * c;
    c = h[0] >> 51; h[0] &= FE_M51; h[1] += c;
}

static void fe_add(fe_t h, const fe_t f, const fe_t g) {
    for (int i = 0; i < 5; ++i) h[i] = f[i] + g[i];
    fe_carry(h);
}

static void fe_sub(fe_t h, const fe_t f, const fe_t g) {
    /* f + 2p - g: every limb of 2p is at least as large as a carried limb of g. */
    h[0] = f[0] + 0xfffffffffffdaull - g[0];
    for (int i = 1; i < 5; ++i) h[i] = f[i] + 0xffffffffffffeull - g[i];
    fe_carry(h);
}

/* Five columns of up to 2^112 into a carried element. The fifth column has no term that was
 * multiplied by 19, which is what leaves room for 19 times its carry in a 64-bit word. */
static void fe_reduce_wide(fe_t h, fe_wide_t t0, fe_wide_t t1, fe_wide_t t2, fe_wide_t t3, fe_wide_t t4) {
    t1 += (proven_u64)(t0 >> 51); t2 += (proven_u64)(t1 >> 51); t3 += (proven_u64)(t2 >> 51); t4 += (proven_u64)(t3 >> 51);
    proven_u64 r0 = ((proven_u64)t0 & FE_M51) + 19 * (proven_u64)(t4 >> 51), r1 = (proven_u64)t1 & FE_M51;
    r1 += r0 >> 51; r0 &= FE_M51;
    h[0] = r0; h[1] = r1; h[2] = (proven_u64)t2 & FE_M51; h[3] = (proven_u64)t3 & FE_M51; h[4] = (proven_u64)t4 & FE_M51;
}

static void fe_mul(fe_t h, const fe_t f, const fe_t g) {
    /* A limb above the fifth wraps to 19 times itself: 2^255 = 19 modulo p. */
    const proven_u64 f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3], f4 = f[4];
    const proven_u64 g0 = g[0], g1 = g[1], g2 = g[2], g3 = g[3], g4 = g[4];
    const proven_u64 g1_19 = g1 * 19, g2_19 = g2 * 19, g3_19 = g3 * 19, g4_19 = g4 * 19;
    fe_wide_t t0 = (fe_wide_t)f0 * g0 + (fe_wide_t)f1 * g4_19 + (fe_wide_t)f2 * g3_19 + (fe_wide_t)f3 * g2_19 + (fe_wide_t)f4 * g1_19;
    fe_wide_t t1 = (fe_wide_t)f0 * g1 + (fe_wide_t)f1 * g0 + (fe_wide_t)f2 * g4_19 + (fe_wide_t)f3 * g3_19 + (fe_wide_t)f4 * g2_19;
    fe_wide_t t2 = (fe_wide_t)f0 * g2 + (fe_wide_t)f1 * g1 + (fe_wide_t)f2 * g0 + (fe_wide_t)f3 * g4_19 + (fe_wide_t)f4 * g3_19;
    fe_wide_t t3 = (fe_wide_t)f0 * g3 + (fe_wide_t)f1 * g2 + (fe_wide_t)f2 * g1 + (fe_wide_t)f3 * g0 + (fe_wide_t)f4 * g4_19;
    fe_wide_t t4 = (fe_wide_t)f0 * g4 + (fe_wide_t)f1 * g3 + (fe_wide_t)f2 * g2 + (fe_wide_t)f3 * g1 + (fe_wide_t)f4 * g0;
    fe_reduce_wide(h, t0, t1, t2, t3, t4);
}

static void fe_sq(fe_t h, const fe_t f) {
    /* The same columns with the equal products taken once and doubled: fifteen multiplications. */
    const proven_u64 f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3], f4 = f[4];
    const proven_u64 d0 = 2 * f0, d1 = 2 * f1, d2 = 2 * f2, d3 = 2 * f3, f3_19 = 19 * f3, f4_19 = 19 * f4;
    fe_wide_t t0 = (fe_wide_t)f0 * f0 + (fe_wide_t)d1 * f4_19 + (fe_wide_t)d2 * f3_19;
    fe_wide_t t1 = (fe_wide_t)d0 * f1 + (fe_wide_t)d2 * f4_19 + (fe_wide_t)f3 * f3_19;
    fe_wide_t t2 = (fe_wide_t)d0 * f2 + (fe_wide_t)f1 * f1 + (fe_wide_t)d3 * f4_19;
    fe_wide_t t3 = (fe_wide_t)d0 * f3 + (fe_wide_t)d1 * f2 + (fe_wide_t)f4 * f4_19;
    fe_wide_t t4 = (fe_wide_t)d0 * f4 + (fe_wide_t)d1 * f3 + (fe_wide_t)f2 * f2;
    fe_reduce_wide(h, t0, t1, t2, t3, t4);
}

static void fe_mul_small(fe_t h, const fe_t f, proven_u32 k) {
    fe_reduce_wide(h, (fe_wide_t)f[0] * k, (fe_wide_t)f[1] * k, (fe_wide_t)f[2] * k, (fe_wide_t)f[3] * k, (fe_wide_t)f[4] * k);
}

static void fe_frombytes(fe_t h, const proven_byte_t s[32]) {
    proven_u64 w[4];
    for (int i = 0; i < 4; ++i) {
        w[i] = 0;
        for (int b = 0; b < 8; ++b) w[i] |= (proven_u64)s[8 * i + b] << (8 * b);
    }
    h[0] = w[0] & FE_M51;
    h[1] = ((w[0] >> 51) | (w[1] << 13)) & FE_M51;
    h[2] = ((w[1] >> 38) | (w[2] << 26)) & FE_M51;
    h[3] = ((w[2] >> 25) | (w[3] << 39)) & FE_M51;
    h[4] = (w[3] >> 12) & FE_M51;             /* the 256th bit is not part of the number */
}

static void fe_tobytes(proven_byte_t s[32], const fe_t f) {
    fe_t h;
    for (int i = 0; i < 5; ++i) h[i] = f[i];
    fe_carry(h);
    /* q = floor((h + 19) / 2^255): 1 exactly when h >= p. Then h + 19q - q 2^255 is h mod p. */
    proven_u64 q = 19;
    for (int i = 0; i < 5; ++i) q = (h[i] + q) >> 51;
    h[0] += 19 * q;
    for (int i = 0; i < 4; ++i) { proven_u64 c = h[i] >> 51; h[i] &= FE_M51; h[i + 1] += c; }
    h[4] &= FE_M51;
    const proven_u64 w[4] = { h[0] | (h[1] << 51), (h[1] >> 13) | (h[2] << 38), (h[2] >> 26) | (h[3] << 25), (h[3] >> 39) | (h[4] << 12) };
    for (int i = 0; i < 4; ++i) {
        for (int b = 0; b < 8; ++b) s[8 * i + b] = (proven_byte_t)(w[i] >> (8 * b));
    }
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)h, .size = sizeof h });
}

#else

/* ---- the field on ten limbs of 26 and 25 bits ---- */

typedef proven_u32 fe_limb_t;
#define FE_LIMBS 10
typedef fe_limb_t fe_t[FE_LIMBS];

static const fe_t FE_D = { 0x35978a3, 0x0d37284, 0x3156ebd, 0x06a0a0e, 0x001c029, 0x179e898, 0x3a03cbb, 0x1ce7198, 0x2e2b6ff, 0x1480db3 };
static const fe_t FE_D2 = { 0x2b2f159, 0x1a6e509, 0x22add7a, 0x0d4141d, 0x0038052, 0x0f3d130, 0x3407977, 0x19ce331, 0x1c56dff, 0x0901b67 };
static const fe_t FE_SQRTM1 = { 0x20ea0b0, 0x186c9d2, 0x08f189d, 0x035697f, 0x0bd0c60, 0x1fbd7a7, 0x2804c9e, 0x1e16569, 0x004fc1d, 0x0ae0c92 };
static const fe_t ED_BX = { 0x325d51a, 0x18b5823, 0x0f6592a, 0x104a92d, 0x1a4b31d, 0x1d6dc5c, 0x27118fe, 0x07fd814, 0x13cd6e5, 0x085a4db };
static const fe_t ED_BY = { 0x2666658, 0x1999999, 0x0cccccc, 0x1333333, 0x1999999, 0x0666666, 0x3333333, 0x0cccccc, 0x2666666, 0x1999999 };

#define FE_WIDTH(i) (((i) & 1) ? 25 : 26)
#define FE_MASK(i) (((i) & 1) ? 0x1ffffffu : 0x3ffffffu)

static void fe_carry(fe_t h) {
    proven_u32 c;
    for (int i = 0; i < 9; ++i) { c = h[i] >> FE_WIDTH(i); h[i] &= FE_MASK(i); h[i + 1] += c; }
    c = h[9] >> 25; h[9] &= 0x1ffffffu; h[0] += 19 * c;
    c = h[0] >> 26; h[0] &= 0x3ffffffu; h[1] += c;
}

static void fe_add(fe_t h, const fe_t f, const fe_t g) {
    for (int i = 0; i < 10; ++i) h[i] = f[i] + g[i];
    fe_carry(h);
}

static void fe_sub(fe_t h, const fe_t f, const fe_t g) {
    /* f + 2p - g: every limb of 2p is at least as large as a carried limb of g. */
    h[0] = f[0] + 0x7ffffdau - g[0];
    for (int i = 1; i < 10; ++i) h[i] = f[i] + ((i & 1) ? 0x3fffffeu : 0x7fffffeu) - g[i];
    fe_carry(h);
}

static void fe_reduce_wide(fe_t h, proven_u64 t[19]) {
    for (int k = 18; k >= 10; --k) t[k - 10] += 19 * t[k];
    proven_u64 c;
    for (int i = 0; i < 9; ++i) { c = t[i] >> FE_WIDTH(i); t[i] &= FE_MASK(i); t[i + 1] += c; }
    c = t[9] >> 25; t[9] &= 0x1ffffffu; t[0] += 19 * c;
    c = t[0] >> 26; t[0] &= 0x3ffffffu; t[1] += c;
    for (int i = 0; i < 10; ++i) h[i] = (proven_u32)t[i];
}

static void fe_mul(fe_t h, const fe_t f, const fe_t g) {
    proven_u64 t[19];
    for (int k = 0; k < 19; ++k) t[k] = 0;
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            /* Limb i sits at bit ceil(25.5 i): two odd limbs meet one bit above their column. */
            proven_u64 m = (proven_u64)f[i] * g[j];
            t[i + j] += m << (i & j & 1);
        }
    }
    fe_reduce_wide(h, t);
}

static void fe_sq(fe_t h, const fe_t f) { fe_mul(h, f, f); }

static void fe_mul_small(fe_t h, const fe_t f, proven_u32 k) {
    proven_u64 t[19];
    for (int i = 0; i < 10; ++i) t[i] = (proven_u64)f[i] * k;
    for (int i = 10; i < 19; ++i) t[i] = 0;
    fe_reduce_wide(h, t);
}

static void fe_frombytes(fe_t h, const proven_byte_t s[32]) {
    int off = 0;
    for (int i = 0; i < 10; ++i) {
        proven_u64 v = 0;
        for (int b = 0; b < 5; ++b) {
            int at = off / 8 + b;
            if (at < 32) v |= (proven_u64)s[at] << (8 * b);
        }
        h[i] = (proven_u32)(v >> (off % 8)) & FE_MASK(i);
        off += FE_WIDTH(i);
    }
}

static void fe_tobytes(proven_byte_t s[32], const fe_t f) {
    fe_t h;
    for (int i = 0; i < 10; ++i) h[i] = f[i];
    fe_carry(h);
    /* q = floor((h + 19) / 2^255): 1 exactly when h >= p. Then h + 19q - q 2^255 is h mod p. */
    proven_u32 q = 19;
    for (int i = 0; i < 10; ++i) q = (h[i] + q) >> FE_WIDTH(i);
    h[0] += 19 * q;
    for (int i = 0; i < 9; ++i) { proven_u32 c = h[i] >> FE_WIDTH(i); h[i] &= FE_MASK(i); h[i + 1] += c; }
    h[9] &= 0x1ffffffu;
    proven_u64 acc = 0;
    int bits = 0, at = 0;
    for (int i = 0; i < 10; ++i) {
        acc |= (proven_u64)h[i] << bits;
        bits += FE_WIDTH(i);
        while (bits >= 8) { s[at++] = (proven_byte_t)acc; acc >>= 8; bits -= 8; }
    }
    s[at] = (proven_byte_t)acc;           /* the last 7 bits */
}

#endif

/* ---- what is written once, over either field ---- */

static void fe_copy(fe_t h, const fe_t f) { for (int i = 0; i < FE_LIMBS; ++i) h[i] = f[i]; }
static void fe_zero(fe_t h) { for (int i = 0; i < FE_LIMBS; ++i) h[i] = 0; }
static void fe_one(fe_t h) { fe_zero(h); h[0] = 1; }

static void fe_sqn(fe_t h, const fe_t f, int n) {
    fe_sq(h, f);
    for (int i = 1; i < n; ++i) fe_sq(h, h);
}

/* z^(2^250 - 1) into t250, z^11 into t11: the shared front of the two exponentiations. */
static void fe_pow_front(fe_t t250, fe_t t11, const fe_t z) {
    fe_t t0, t1, t2, t3;
    fe_sq(t0, z);                         /* 2 */
    fe_sqn(t1, t0, 2);                    /* 8 */
    fe_mul(t1, z, t1);                    /* 9 */
    fe_mul(t0, t0, t1);                   /* 11 */
    fe_copy(t11, t0);
    fe_sq(t2, t0);                        /* 22 */
    fe_mul(t1, t1, t2);                   /* 2^5 - 1 */
    fe_sqn(t2, t1, 5); fe_mul(t1, t2, t1);    /* 2^10 - 1 */
    fe_sqn(t2, t1, 10); fe_mul(t2, t2, t1);   /* 2^20 - 1 */
    fe_sqn(t3, t2, 20); fe_mul(t2, t3, t2);   /* 2^40 - 1 */
    fe_sqn(t2, t2, 10); fe_mul(t1, t2, t1);   /* 2^50 - 1 */
    fe_sqn(t2, t1, 50); fe_mul(t2, t2, t1);   /* 2^100 - 1 */
    fe_sqn(t3, t2, 100); fe_mul(t2, t3, t2);  /* 2^200 - 1 */
    fe_sqn(t2, t2, 50); fe_mul(t250, t2, t1); /* 2^250 - 1 */
}

static void fe_invert(fe_t out, const fe_t z) {
    fe_t t, t11;
    fe_pow_front(t, t11, z);
    fe_sqn(t, t, 5);                      /* 2^255 - 32 */
    fe_mul(out, t, t11);                  /* 2^255 - 21 = p - 2 */
}

static void fe_pow22523(fe_t out, const fe_t z) {
    fe_t t, t11;
    fe_pow_front(t, t11, z);
    fe_sqn(t, t, 2);                      /* 2^252 - 4 */
    fe_mul(out, t, z);                    /* 2^252 - 3 = (p - 5) / 8 */
}

/* Swap f and g when bit is 1, by mask. */
static void fe_cswap(fe_t f, fe_t g, proven_u32 bit) {
    fe_limb_t mask = (fe_limb_t)0 - (fe_limb_t)bit;
    for (int i = 0; i < FE_LIMBS; ++i) { fe_limb_t x = (f[i] ^ g[i]) & mask; f[i] ^= x; g[i] ^= x; }
}

static bool fe_equal(const fe_t f, const fe_t g) {
    proven_byte_t a[32], b[32];
    fe_tobytes(a, f); fe_tobytes(b, g);
    return proven_mem_equal_ct((proven_mem_view_t){ .ptr = a, .size = 32 }, (proven_mem_view_t){ .ptr = b, .size = 32 });
}

static void wipe(void *p, proven_size_t n) { proven_mem_wipe((proven_mem_mut_t){ .ptr = p, .size = n }); }

/* ---- X25519 ---- */

bool proven_crypto_x25519(proven_byte_t out[32], const proven_byte_t scalar[32], const proven_byte_t u[32]) {
    proven_byte_t k[32];
    for (int i = 0; i < 32; ++i) k[i] = scalar[i];
    k[0] &= 248; k[31] &= 127; k[31] |= 64;
    fe_t x1, x2, z2, x3, z3, a, aa, b, bb, e, c, d, da, cb, t;
    fe_frombytes(x1, u);
    fe_one(x2); fe_zero(z2); fe_copy(x3, x1); fe_one(z3);
    proven_u32 swap = 0;
    for (int pos = 254; pos >= 0; --pos) {
        proven_u32 bit = (proven_u32)(k[pos / 8] >> (pos % 8)) & 1u;
        swap ^= bit;
        fe_cswap(x2, x3, swap); fe_cswap(z2, z3, swap);
        swap = bit;
        fe_add(a, x2, z2); fe_sq(aa, a);
        fe_sub(b, x2, z2); fe_sq(bb, b);
        fe_sub(e, aa, bb);
        fe_add(c, x3, z3); fe_sub(d, x3, z3);
        fe_mul(da, d, a); fe_mul(cb, c, b);
        fe_add(t, da, cb); fe_sq(x3, t);
        fe_sub(t, da, cb); fe_sq(t, t); fe_mul(z3, x1, t);
        fe_mul(x2, aa, bb);
        fe_mul_small(t, e, 121665); fe_add(t, aa, t); fe_mul(z2, e, t);
    }
    fe_cswap(x2, x3, swap); fe_cswap(z2, z3, swap);
    fe_invert(z2, z2);
    fe_mul(x2, x2, z2);
    fe_tobytes(out, x2);
    proven_byte_t acc = 0;
    for (int i = 0; i < 32; ++i) acc |= out[i];
    wipe(k, sizeof k); wipe(x2, sizeof(fe_t)); wipe(z2, sizeof(fe_t)); wipe(x3, sizeof(fe_t)); wipe(z3, sizeof(fe_t));
    wipe(a, sizeof(fe_t)); wipe(b, sizeof(fe_t)); wipe(aa, sizeof(fe_t)); wipe(bb, sizeof(fe_t)); wipe(e, sizeof(fe_t));
    wipe(c, sizeof(fe_t)); wipe(d, sizeof(fe_t)); wipe(da, sizeof(fe_t)); wipe(cb, sizeof(fe_t)); wipe(t, sizeof(fe_t));
    /* Whether the shared value is zero is revealed, as it must be: the exchange is refused. */
    bool nonzero = acc != 0;
    PROVEN_CT_PUBLIC(&nonzero, sizeof nonzero);
    return nonzero;
}

void proven_crypto_x25519_public(proven_byte_t out[32], const proven_byte_t scalar[32]) {
    proven_byte_t base[32] = { 9 };
    (void)proven_crypto_x25519(out, scalar, base);     /* never zero for the base point */
}

/* ---- Ed25519 ---- */

/* Extended coordinates on -x^2 + y^2 = 1 + d x^2 y^2: x = X/Z, y = Y/Z, T = XY/Z. The addition
 * below is complete on this curve: it is right for every pair of points. */
typedef struct { fe_t x, y, z, t; } proven_ge_t;
#define GE_LIMBS (4 * FE_LIMBS)

static void ge_identity(proven_ge_t *p) { fe_zero(p->x); fe_one(p->y); fe_one(p->z); fe_zero(p->t); }

static void ge_add(proven_ge_t *r, const proven_ge_t *p, const proven_ge_t *q) {
    fe_t a, b, c, d, e, f, g, h, t;
    fe_sub(a, p->y, p->x); fe_sub(t, q->y, q->x); fe_mul(a, a, t);
    fe_add(b, p->y, p->x); fe_add(t, q->y, q->x); fe_mul(b, b, t);
    fe_mul(c, p->t, q->t); fe_mul(c, c, FE_D2);
    fe_mul(d, p->z, q->z); fe_add(d, d, d);
    fe_sub(e, b, a); fe_sub(f, d, c); fe_add(g, d, c); fe_add(h, b, a);
    fe_mul(r->x, e, f); fe_mul(r->y, g, h); fe_mul(r->t, e, h); fe_mul(r->z, f, g);
}

static void ge_double(proven_ge_t *r, const proven_ge_t *p) {
    fe_t a, b, c, e, f, g, h, t;
    fe_sq(a, p->x); fe_sq(b, p->y);
    fe_sq(c, p->z); fe_add(c, c, c);
    fe_add(t, p->x, p->y); fe_sq(e, t); fe_sub(e, e, a); fe_sub(e, e, b);
    fe_sub(g, b, a);
    fe_sub(f, g, c);
    fe_zero(t); fe_sub(h, t, a); fe_sub(h, h, b);
    fe_mul(r->x, e, f); fe_mul(r->y, g, h); fe_mul(r->t, e, h); fe_mul(r->z, f, g);
}

/* r = k * p for a 256-bit little-endian scalar (SECRET): a fixed window of four bits, every
 * table entry read on every lookup. */
static void ge_mul(proven_ge_t *r, const proven_byte_t k[32], const proven_ge_t *p) {
    proven_ge_t table[16], acc, pick;
    ge_identity(&table[0]);
    table[1] = *p;
    for (int i = 2; i < 16; ++i) ge_add(&table[i], &table[i - 1], p);
    ge_identity(&acc);
    for (int i = 63; i >= 0; --i) {
        for (int d = 0; d < 4; ++d) ge_double(&acc, &acc);
        proven_u32 nib = (proven_u32)(k[i / 2] >> (4 * (i % 2))) & 15u;
        fe_limb_t *dst = (fe_limb_t *)&pick;
        for (proven_size_t j = 0; j < GE_LIMBS; ++j) dst[j] = 0;
        for (proven_u32 t = 0; t < 16; ++t) {
            proven_u32 x = t ^ nib;
            fe_limb_t mask = (fe_limb_t)0 - (fe_limb_t)((((x | ((proven_u32)0 - x)) >> 31) ^ 1u));  /* all ones when t == nib */
            const fe_limb_t *src = (const fe_limb_t *)&table[t];
            for (proven_size_t j = 0; j < GE_LIMBS; ++j) dst[j] |= src[j] & mask;
        }
        ge_add(&acc, &acc, &pick);
    }
    *r = acc;
    wipe(table, sizeof table); wipe(&acc, sizeof acc); wipe(&pick, sizeof pick);
}

static void ge_encode(proven_byte_t s[32], const proven_ge_t *p) {
    fe_t zi, x, y;
    proven_byte_t xb[32];
    fe_invert(zi, p->z);
    fe_mul(x, p->x, zi); fe_mul(y, p->y, zi);
    fe_tobytes(s, y);
    fe_tobytes(xb, x);
    s[31] ^= (proven_byte_t)((xb[0] & 1) << 7);
    wipe(zi, sizeof zi); wipe(x, sizeof x); wipe(y, sizeof y); wipe(xb, sizeof xb);
}

/* A point from its encoding. Public data only. Refuses a y that is not below p, a y with no
 * x on the curve, and x = 0 with the sign bit set. */
static bool ge_decode(proven_ge_t *p, const proven_byte_t s[32]) {
    proven_byte_t yb[32], back[32];
    for (int i = 0; i < 32; ++i) yb[i] = s[i];
    proven_u32 sign = yb[31] >> 7;
    yb[31] &= 127;
    fe_frombytes(p->y, yb);
    fe_tobytes(back, p->y);
    for (int i = 0; i < 32; ++i) if (back[i] != yb[i]) return false;
    fe_t u, v, v3, x, vxx, one, t;
    fe_one(one); fe_one(p->z);
    fe_sq(u, p->y); fe_mul(v, u, FE_D);
    fe_sub(u, u, one);                     /* y^2 - 1 */
    fe_add(v, v, one);                     /* d y^2 + 1 */
    /* x = u v^3 (u v^7)^((p-5)/8) */
    fe_sq(v3, v); fe_mul(v3, v3, v);
    fe_sq(x, v3); fe_mul(x, x, v); fe_mul(x, x, u);
    fe_pow22523(x, x);
    fe_mul(x, x, v3); fe_mul(x, x, u);
    fe_sq(vxx, x); fe_mul(vxx, vxx, v);
    if (!fe_equal(vxx, u)) {
        fe_zero(t); fe_sub(t, t, u);
        if (!fe_equal(vxx, t)) return false;
        fe_mul(x, x, FE_SQRTM1);
    }
    proven_byte_t xb[32];
    fe_tobytes(xb, x);
    proven_byte_t any = 0;
    for (int i = 0; i < 32; ++i) any |= xb[i];
    if (any == 0 && sign) return false;
    if ((proven_u32)(xb[0] & 1) != sign) { fe_zero(t); fe_sub(x, t, x); }
    fe_copy(p->x, x);
    fe_mul(p->t, p->x, p->y);
    return true;
}

static void ed_base(proven_ge_t *b) {
    fe_copy(b->x, ED_BX); fe_copy(b->y, ED_BY); fe_one(b->z); fe_mul(b->t, ED_BX, ED_BY);
}

/* The base point's table for a signed comb of five teeth, d = 51 apart (ceil(253 / 5): every
 * scalar here is below L < 2^253). Entry i is 2^(4d) B + the sum over b < 4 of (+1 if bit b of
 * i is set, else -1) 2^(b d) B, as affine x then y, 32 bytes little-endian each. Made by
 * RFC 8032's integer arithmetic outside this code; proven_crypto_ed25519_test_table
 * recomputes each with the general multiplication. */
#define ED_COMB_TEETH 5
#define ED_COMB_D 51
#define ED_GTAB_POINTS 16
static const proven_byte_t ED_GTAB[16][64] = {
    { /* 0 */
        0xd8,0x40,0xd0,0xbb,0xab,0x67,0xef,0x3b,0xee,0x30,0x2c,0x17,0xec,0xcf,0x9b,0x3e,
        0x2d,0xaa,0xea,0x6e,0x24,0x31,0x6b,0xe8,0xfb,0x7f,0x49,0xe2,0xf5,0x91,0xc0,0x48,
        0x90,0xa8,0x57,0x50,0x0d,0xfa,0xca,0x52,0x78,0xd7,0x95,0x4b,0x39,0xb3,0xcf,0x9f,
        0x48,0x6b,0x2b,0x5d,0xf2,0xbe,0x33,0x06,0xe3,0x4f,0x64,0x7b,0x3e,0x76,0xee,0x0a
    },
    { /* 1 */
        0x4e,0x3f,0x04,0xa2,0xd7,0xbb,0x62,0x3a,0x52,0x58,0xc4,0x81,0xa5,0xde,0xeb,0xb5,
        0xeb,0x8c,0x81,0x27,0xd6,0x1b,0x46,0xcb,0xab,0x47,0x9e,0x62,0x69,0xbf,0xc8,0x1b,
        0x1a,0x5b,0xe8,0x25,0x3a,0x68,0x86,0x3b,0x71,0x59,0xcf,0xfa,0x0e,0x54,0x45,0x22,
        0xed,0xdb,0x6a,0x19,0x53,0x50,0xbc,0x61,0x73,0xe5,0xbc,0xc7,0xe5,0x9b,0x69,0x17
    },
    { /* 2 */
        0xc6,0xf7,0x21,0xd8,0x0f,0xe0,0x3b,0x66,0xfb,0x34,0x26,0x5b,0x89,0xff,0xe5,0x90,
        0xab,0x7c,0x87,0xfe,0x14,0xf5,0xec,0x5e,0xbc,0xab,0x8b,0x76,0xea,0x04,0xe5,0x69,
        0x1f,0xb2,0x4e,0xda,0x18,0x0d,0xe1,0xb0,0x94,0xcf,0x92,0xd2,0xea,0x4b,0xe7,0xd3,
        0x41,0x5e,0x71,0xb2,0x05,0x81,0xba,0x44,0x77,0x82,0xc4,0xbb,0x86,0x94,0x54,0x25
    },
    { /* 3 */
        0x5c,0xdd,0xad,0x63,0x63,0xa4,0x9e,0x4b,0x27,0x68,0xb6,0x2a,0xe9,0xab,0x14,0x94,
        0x0f,0xe4,0xf1,0x7f,0xc7,0x2d,0xe2,0x56,0x67,0xea,0x8d,0x00,0x89,0xab,0x99,0x1c,
        0x2d,0x50,0xc7,0x8a,0x24,0xd3,0x71,0x06,0x1e,0xc1,0x46,0x1f,0x04,0xa0,0x5b,0x08,
        0x53,0xd3,0x0d,0x84,0x70,0x26,0xaa,0x5b,0xdf,0x4e,0x13,0xbd,0x7b,0xdd,0x35,0x3b
    },
    { /* 4 */
        0xe4,0x53,0xd6,0x90,0xaf,0x73,0xa2,0xea,0xad,0x4a,0x32,0x70,0xa9,0xa6,0x64,0xbf,
        0x02,0xb6,0xbb,0x1a,0x81,0xdb,0x18,0xf8,0x7d,0x1e,0xeb,0xe0,0xdf,0xcc,0xfe,0x1d,
        0x20,0x9f,0x55,0xf2,0x3b,0xcb,0xbb,0x3b,0x89,0xcf,0x36,0xbd,0x17,0x65,0x00,0xc2,
        0x1a,0x1b,0xde,0xdb,0xe2,0xe7,0x8f,0xd8,0xa5,0x66,0x5d,0x3e,0xa1,0xe3,0x6f,0x6b
    },
    { /* 5 */
        0x56,0x8f,0x40,0xb6,0x75,0x5b,0xf5,0xb4,0x69,0xc7,0x14,0x2a,0xa4,0x32,0x4a,0xbe,
        0x46,0x2a,0xfd,0x28,0xa5,0xe2,0x7c,0x12,0xf6,0x5d,0xb1,0xfb,0x6e,0x35,0x98,0x1f,
        0xf9,0x87,0xc0,0x01,0x62,0xa9,0xcd,0x1c,0x7a,0x70,0x00,0xdf,0x4b,0x8d,0x01,0x9b,
        0x12,0x07,0x3e,0xce,0xb1,0xf6,0x16,0x3b,0x66,0xb7,0x41,0x11,0x81,0x41,0xe9,0x1a
    },
    { /* 6 */
        0xfe,0xe6,0x07,0x9e,0x49,0xf5,0x81,0x19,0x7d,0x73,0x57,0xff,0xb9,0xfc,0x4a,0x5e,
        0x6d,0x77,0xe7,0xbd,0x93,0xc0,0x93,0x8a,0xbf,0xc3,0xf8,0x55,0xa0,0x9a,0x0b,0x7e,
        0xfe,0x26,0x9a,0x83,0x81,0x02,0xb7,0x94,0xc0,0xf7,0x2e,0xeb,0x56,0x17,0x43,0x79,
        0x78,0xfb,0x78,0x03,0x3b,0x87,0x91,0x7d,0xa4,0xc7,0x39,0x00,0x3e,0x1a,0xac,0x77
    },
    { /* 7 */
        0xf6,0x3b,0x11,0x00,0xe1,0x3a,0xe9,0xb8,0xd9,0xd0,0xe0,0xd2,0x5a,0x5b,0x0b,0x60,
        0x87,0xc0,0xa8,0x0f,0x33,0x1d,0x56,0x62,0xbb,0xcc,0x8f,0xbe,0xbc,0x8e,0xf9,0x34,
        0xa6,0x9b,0xba,0xf3,0xba,0x9a,0x26,0x8a,0x41,0x30,0xca,0x92,0xce,0x25,0x65,0x98,
        0x84,0xab,0xc4,0x1b,0x68,0xb7,0x1c,0xb9,0x24,0xa5,0xba,0x68,0xab,0x7f,0xf0,0x39
    },
    { /* 8 */
        0x83,0xe9,0xd6,0x43,0x17,0x2f,0x5f,0xeb,0x3d,0xb4,0x5d,0x23,0x06,0xf0,0x71,0xc1,
        0xfb,0xfb,0x10,0x51,0x74,0xb6,0xe7,0x50,0xdf,0x52,0xf8,0x10,0x10,0x29,0x40,0x24,
        0xf6,0x35,0xe5,0xb3,0xeb,0x22,0x42,0x76,0x77,0x69,0xc3,0x98,0x14,0x0f,0xda,0x79,
        0x27,0xc2,0xe3,0x75,0x8b,0x6f,0xf7,0xaa,0x4f,0x69,0x3c,0x0b,0x1b,0xb7,0xf5,0x7b
    },
    { /* 9 */
        0xd7,0x14,0x22,0x72,0x8e,0x1f,0x48,0x4c,0x16,0x72,0x68,0xde,0xd2,0xec,0x4b,0x45,
        0x0f,0xc5,0x28,0xc4,0xa8,0xa2,0x87,0x2c,0xed,0xe0,0xe1,0x30,0xc7,0xa8,0x23,0x48,
        0x62,0xc1,0xcd,0x3f,0xbd,0xa1,0x2b,0xb2,0xf5,0x6e,0x91,0x7c,0x8f,0x82,0x71,0xae,
        0xb5,0xb9,0x99,0x51,0x5f,0x04,0x40,0x66,0x27,0x55,0xf7,0x05,0xb7,0x9a,0x5b,0x65
    },
    { /* 10 */
        0x15,0x56,0x64,0xe8,0x1c,0xd1,0x44,0xe1,0x14,0x39,0xe5,0xbb,0x50,0xbc,0x0b,0xc5,
        0x2a,0xa5,0x47,0x38,0x98,0x69,0xeb,0xb7,0xe4,0x99,0x37,0xac,0x5b,0xb6,0x5d,0x2f,
        0x35,0x4a,0x34,0xc9,0x18,0x2d,0xbe,0x70,0x4f,0xfb,0xdb,0x7f,0xa7,0x82,0xc4,0xf0,
        0x98,0xe6,0xea,0x7e,0x78,0x35,0x17,0xaf,0xd6,0x59,0x31,0xc9,0x13,0x8e,0xfe,0x76
    },
    { /* 11 */
        0x69,0x69,0xd5,0xa0,0x67,0x56,0x49,0x9a,0xa7,0xea,0xcb,0x3d,0xf3,0xd5,0x4a,0x51,
        0x55,0x81,0x40,0xcb,0x90,0x9c,0x6c,0xfa,0x24,0xb6,0xda,0xdf,0x84,0xcb,0xc1,0x38,
        0x64,0xd3,0x83,0xc3,0x8b,0x45,0xae,0x67,0x5c,0x70,0xb4,0x53,0x76,0x5d,0x71,0xc7,
        0x46,0x8a,0x65,0x2e,0x9a,0x02,0x9a,0xb0,0x22,0x8a,0x91,0xcb,0xe8,0x6f,0x17,0x7e
    },
    { /* 12 */
        0x49,0x4f,0xf2,0x10,0xfa,0x8c,0x69,0x53,0x16,0x70,0xa4,0xfa,0x98,0x02,0x0c,0x35,
        0x7e,0xb5,0x37,0x10,0xa3,0xfc,0xbe,0xa9,0x2b,0x98,0x44,0x39,0x22,0x80,0x89,0x61,
        0x9c,0xbc,0xf6,0xe8,0x33,0x70,0xb9,0x7d,0x4d,0x14,0x54,0xea,0x13,0x3f,0x17,0x98,
        0xce,0x09,0x87,0x7b,0x0e,0x20,0xb6,0xfb,0xc5,0xee,0x2c,0x72,0xbf,0x4e,0x70,0x78
    },
    { /* 13 */
        0xf4,0x37,0x67,0xfb,0x64,0xa0,0x8c,0x55,0xe3,0xa2,0x36,0x6c,0xfb,0x70,0xcb,0x8d,
        0xa7,0x32,0x62,0x27,0x56,0x47,0x52,0x67,0x04,0x3e,0x6c,0x6a,0x7b,0x09,0xe1,0x1f,
        0x9a,0xe9,0xff,0x81,0x01,0xe4,0x19,0x0b,0x35,0x7d,0x95,0x4d,0x4d,0x27,0xd2,0xe4,
        0x21,0x1a,0x0b,0x57,0x2e,0x74,0xf5,0x71,0x67,0x62,0x8a,0x09,0x4f,0xe0,0xbd,0x27
    },
    { /* 14 */
        0xc5,0xbf,0x77,0x1b,0x82,0xc8,0xa1,0x26,0x0c,0xb8,0x90,0x9a,0x58,0xc4,0xa1,0x58,
        0xdd,0xd0,0x9b,0xb0,0xfc,0x37,0x67,0x47,0x2a,0xc5,0x23,0x9a,0x81,0xee,0x1d,0x0d,
        0x41,0xfe,0x0b,0x70,0x00,0x76,0x73,0x26,0x79,0x0f,0xa1,0xa8,0xc2,0x8e,0xee,0xbd,
        0x6a,0x51,0x65,0xfc,0xd8,0xe5,0xb7,0xaa,0x6d,0x2e,0x91,0x44,0x84,0x19,0x19,0x32
    },
    { /* 15 */
        0xa7,0xfb,0xbd,0x49,0x15,0x6c,0x9f,0xb8,0xdb,0xe5,0xdf,0x69,0xea,0xc5,0xd4,0x61,
        0x4c,0xf5,0x66,0x7a,0x69,0xfb,0x5d,0x57,0xb5,0xf6,0xf6,0x1a,0xf3,0x55,0xbe,0x08,
        0x4d,0xcc,0xa0,0xba,0x65,0xea,0x5d,0x58,0xdb,0xe5,0xd7,0xdd,0xda,0xa6,0xe2,0x74,
        0x1c,0xf8,0x4e,0xec,0x21,0xfa,0xb5,0xe8,0x20,0xfb,0x0d,0x58,0xc0,0xd3,0x79,0x55
    }
};

/* An affine point as (y + x, y - x, 2 d x y): what a mixed addition reads. */
typedef struct { fe_t ypx, ymx, xy2d; } proven_ge_niels_t;
/* A projective point as (Y + X, Y - X, Z, 2 d T): what a full addition reads. */
typedef struct { fe_t ypx, ymx, z, t2d; } proven_ge_cached_t;

static void ge_niels_load(proven_ge_niels_t *n, const proven_byte_t e[64]) {
    fe_t x, y;
    fe_frombytes(x, e); fe_frombytes(y, e + 32);
    fe_add(n->ypx, y, x); fe_sub(n->ymx, y, x);
    fe_mul(n->xy2d, x, y); fe_mul(n->xy2d, n->xy2d, FE_D2);
}

/* p + q for q affine, seven multiplications; complete, like ge_add. `r` may be `p`. */
static void ge_madd(proven_ge_t *r, const proven_ge_t *p, const proven_ge_niels_t *q) {
    fe_t a, b, c, d, e, f, g, h;
    fe_sub(a, p->y, p->x); fe_mul(a, a, q->ymx);
    fe_add(b, p->y, p->x); fe_mul(b, b, q->ypx);
    fe_mul(c, p->t, q->xy2d);
    fe_add(d, p->z, p->z);
    fe_sub(e, b, a); fe_sub(f, d, c); fe_add(g, d, c); fe_add(h, b, a);
    fe_mul(r->x, e, f); fe_mul(r->y, g, h); fe_mul(r->t, e, h); fe_mul(r->z, f, g);
}

/* A doubling that leaves T alone: for a doubling another doubling follows, which does not
 * read it. */
static void ge_double_xyz(proven_ge_t *r, const proven_ge_t *p) {
    fe_t a, b, c, e, f, g, h, t;
    fe_sq(a, p->x); fe_sq(b, p->y);
    fe_sq(c, p->z); fe_add(c, c, c);
    fe_add(t, p->x, p->y); fe_sq(e, t); fe_sub(e, e, a); fe_sub(e, e, b);
    fe_sub(g, b, a);
    fe_sub(f, g, c);
    fe_zero(t); fe_sub(h, t, a); fe_sub(h, h, b);
    fe_mul(r->x, e, f); fe_mul(r->y, g, h); fe_mul(r->z, f, g);
}

#define SC_LIMBS PROVEN_CRYPTO_MP_LIMBS(32)

/* The comb's digits of k (below L, plain limbs): k, or L - k when k is even (`even` all ones
 * then), as t = (that - 1) / 2 + 2^254. Bit j + b d of t is digit b of column j. */
static void sc_comb_digits(proven_crypto_limb_t t[SC_LIMBS], const proven_crypto_limb_t k[SC_LIMBS], const proven_crypto_limb_t *l, proven_crypto_limb_t even) {
    const unsigned w = PROVEN_CRYPTO_MP_LIMB_BITS;
    (void)proven_crypto_mp_sub_raw(t, l, k, SC_LIMBS);
    proven_crypto_mp_select(t, t, k, even, SC_LIMBS);
    for (proven_size_t i = 0; i < SC_LIMBS; ++i) t[i] = (t[i] >> 1) | (i + 1 < SC_LIMBS ? t[i + 1] << (w - 1) : 0u);
    t[(ED_COMB_TEETH * ED_COMB_D - 1) / w] |= (proven_crypto_limb_t)1 << ((ED_COMB_TEETH * ED_COMB_D - 1) % w);
}

static proven_u32 sc_bit(const proven_crypto_limb_t *t, proven_size_t pos) {
    return (proven_u32)(t[pos / PROVEN_CRYPTO_MP_LIMB_BITS] >> (pos % PROVEN_CRYPTO_MP_LIMB_BITS)) & 1u;
}

/* k B for k below L in plain limbs (SECRET), by the signed comb: as ec_mul_base in
 * crypto_ec.c - 51 doublings and 51 mixed additions, every column adding the entry its four
 * low digits name (or the negative of their complement's, when its top digit is -1), every
 * entry read on every lookup and one kept by mask. An even k is replaced by L - k, which is
 * odd, and the result negated. */
static void ge_mul_base(proven_ge_t *r, const proven_crypto_limb_t k[SC_LIMBS], const proven_crypto_limb_t *l) {
    proven_ge_niels_t table[ED_GTAB_POINTS], pick;
    proven_crypto_limb_t t[SC_LIMBS];
    fe_t zero, neg;
    proven_ge_t acc;
    for (proven_size_t j = 0; j < ED_GTAB_POINTS; ++j) ge_niels_load(&table[j], ED_GTAB[j]);
    const proven_crypto_limb_t even = (proven_crypto_limb_t)0 - ((k[0] & 1u) ^ 1u);
    sc_comb_digits(t, k, l, even);
    fe_zero(zero);
    ge_identity(&acc);
    for (proven_size_t j = ED_COMB_D; j-- > 0;) {
        ge_double(&acc, &acc);
        proven_u32 low = 0;
        for (proven_size_t b = 0; b < ED_COMB_TEETH - 1; ++b) low |= sc_bit(t, j + b * ED_COMB_D) << b;
        const proven_u32 plus = sc_bit(t, j + (ED_COMB_TEETH - 1) * ED_COMB_D);
        const proven_u32 pm = (proven_u32)0 - plus;                       /* all ones when the top digit is +1 */
        const proven_u32 want = (low & pm) | ((low ^ 15u) & ~pm);
        fe_limb_t *dst = (fe_limb_t *)&pick;
        for (proven_size_t i = 0; i < 3 * FE_LIMBS; ++i) dst[i] = 0;
        for (proven_u32 e = 0; e < ED_GTAB_POINTS; ++e) {
            const proven_u32 x = e ^ want;
            const fe_limb_t mask = (fe_limb_t)0 - (fe_limb_t)(((x | ((proven_u32)0 - x)) >> 31) ^ 1u);     /* all ones when e == want */
            const fe_limb_t *src = (const fe_limb_t *)&table[e];
            for (proven_size_t i = 0; i < 3 * FE_LIMBS; ++i) dst[i] |= src[i] & mask;
        }
        /* -(x, y) is (-x, y): y + x and y - x trade places, 2 d x y changes sign. */
        fe_cswap(pick.ypx, pick.ymx, plus ^ 1u);
        fe_sub(neg, zero, pick.xy2d);
        fe_cswap(pick.xy2d, neg, plus ^ 1u);
        ge_madd(&acc, &acc, &pick);
    }
    fe_sub(neg, zero, acc.x); fe_cswap(acc.x, neg, (proven_u32)(even & 1u));
    fe_sub(neg, zero, acc.t); fe_cswap(acc.t, neg, (proven_u32)(even & 1u));
    *r = acc;
    wipe(t, sizeof t); wipe(&pick, sizeof pick); wipe(neg, sizeof neg); wipe(&acc, sizeof acc);
}

/* ---- Verification, in variable time ----
 *
 * As in crypto_ec.c: a verification has no secret, and the functions named `_vartime` branch
 * and index on what they are given. proven_crypto_ed25519_verify and the suite's hooks are
 * their only callers; scripts/check-vartime.py fails the build otherwise. */

static void ge_to_cached(proven_ge_cached_t *c, const proven_ge_t *p) {
    fe_add(c->ypx, p->y, p->x); fe_sub(c->ymx, p->y, p->x);
    fe_copy(c->z, p->z); fe_mul(c->t2d, p->t, FE_D2);
}

/* p + q, or p - q when `minus`. `r` may be `p`. */
static void ge_add_cached_vartime(proven_ge_t *r, const proven_ge_t *p, const proven_ge_cached_t *q, bool minus) {
    fe_t a, b, c, d, e, f, g, h;
    fe_sub(a, p->y, p->x); fe_mul(a, a, minus ? q->ypx : q->ymx);
    fe_add(b, p->y, p->x); fe_mul(b, b, minus ? q->ymx : q->ypx);
    fe_mul(c, p->t, q->t2d);
    fe_mul(d, p->z, q->z); fe_add(d, d, d);
    fe_sub(e, b, a); fe_add(h, b, a);
    if (minus) { fe_add(f, d, c); fe_sub(g, d, c); }
    else { fe_sub(f, d, c); fe_add(g, d, c); }
    fe_mul(r->x, e, f); fe_mul(r->y, g, h); fe_mul(r->t, e, h); fe_mul(r->z, f, g);
}

/* p + q for q affine, or p - q when `minus`. */
static void ge_madd_vartime(proven_ge_t *r, const proven_ge_t *p, const proven_ge_niels_t *q, bool minus) {
    fe_t a, b, c, d, e, f, g, h;
    fe_sub(a, p->y, p->x); fe_mul(a, a, minus ? q->ypx : q->ymx);
    fe_add(b, p->y, p->x); fe_mul(b, b, minus ? q->ymx : q->ypx);
    fe_mul(c, p->t, q->xy2d);
    fe_add(d, p->z, p->z);
    fe_sub(e, b, a); fe_add(h, b, a);
    if (minus) { fe_add(f, d, c); fe_sub(g, d, c); }
    else { fe_sub(f, d, c); fe_add(g, d, c); }
    fe_mul(r->x, e, f); fe_mul(r->y, g, h); fe_mul(r->t, e, h); fe_mul(r->z, f, g);
}

/* The width-5 NAF of a scalar below 2^253 (32 bytes, little-endian): digit i odd in
 * [-15, 15] or zero, 254 digits. Returns one past the highest nonzero digit. */
static proven_size_t sc_wnaf_vartime(signed char digits[254], const proven_byte_t k[32]) {
    proven_size_t used = 0;
    unsigned carry = 0;
    for (proven_size_t i = 0; i < 254; ++i) digits[i] = 0;
    for (proven_size_t bit = 0; bit < 254;) {
        if (((unsigned)(k[bit / 8] >> (bit % 8)) & 1u) == carry) { ++bit; continue; }
        unsigned word = 0;
        for (unsigned j = 0; j < 5; ++j) {
            const proven_size_t at = bit + j;
            if (at < 256) word |= ((unsigned)(k[at / 8] >> (at % 8)) & 1u) << j;
        }
        word += carry;
        carry = (word >> 4) & 1u;
        digits[bit] = (signed char)((int)word - (int)(carry << 5));
        used = bit + 1;
        bit += 5;
    }
    return used;
}

/* s B + k Q for public s (plain limbs, below L), k (bytes, below L) and Q: one walk of the
 * doublings for both. Q's part reads its width-5 NAF against eight odd multiples of Q; B's
 * part is the signed comb of ge_mul_base, column j added when the walk is at j. */
static void ge_mul2_vartime(proven_ge_t *r, const proven_crypto_limb_t s[SC_LIMBS], const proven_byte_t k[32], const proven_ge_t *q, const proven_crypto_limb_t *l) {
    proven_ge_cached_t table[8];
    proven_ge_niels_t base[ED_GTAB_POINTS];
    signed char naf[254];
    proven_crypto_limb_t t[SC_LIMBS];
    proven_ge_t acc, q2;
    proven_ge_cached_t c2;
    const proven_size_t ktop = sc_wnaf_vartime(naf, k);
    if (ktop > 0) {
        ge_to_cached(&table[0], q);
        ge_double(&q2, q);
        ge_to_cached(&c2, &q2);
        acc = *q;
        for (proven_size_t i = 1; i < 8; ++i) { ge_add_cached_vartime(&acc, &acc, &c2, false); ge_to_cached(&table[i], &acc); }
    }
    const bool spart = !proven_crypto_mp_is_zero(s, SC_LIMBS), negate = (s[0] & 1u) == 0;
    if (spart) {
        for (proven_size_t j = 0; j < ED_GTAB_POINTS; ++j) ge_niels_load(&base[j], ED_GTAB[j]);
        sc_comb_digits(t, s, l, negate ? ~(proven_crypto_limb_t)0 : 0);
    }
    ge_identity(&acc);
    const proven_size_t start = ktop > (spart ? ED_COMB_D : 0) ? ktop : (spart ? ED_COMB_D : 0);
    for (proven_size_t i = start; i-- > 0;) {
        const bool comb = spart && i < ED_COMB_D, wnaf = i < ktop && naf[i] != 0;
        if (comb || wnaf) ge_double(&acc, &acc);
        else ge_double_xyz(&acc, &acc);
        if (comb) {
            unsigned low = 0;
            for (proven_size_t b = 0; b < ED_COMB_TEETH - 1; ++b) low |= sc_bit(t, i + b * ED_COMB_D) << b;
            const bool plus = sc_bit(t, i + (ED_COMB_TEETH - 1) * ED_COMB_D) != 0;
            ge_madd_vartime(&acc, &acc, &base[plus ? low : low ^ 15u], plus == negate);
        }
        if (wnaf) {
            const int dg = naf[i];
            ge_add_cached_vartime(&acc, &acc, &table[(dg > 0 ? dg : -dg) / 2], dg < 0);
        }
    }
    *r = acc;
}


/* A 64-byte little-endian hash, or a 32-byte scalar, reduced mod L. */
static void sc_reduce(const proven_crypto_mp_mod_t *l, proven_crypto_limb_t out[SC_LIMBS], const proven_byte_t *le, proven_size_t n) {
    proven_byte_t be[64];
    for (proven_size_t i = 0; i < n; ++i) be[i] = le[n - 1 - i];
    proven_crypto_mp_reduce_be(l, out, be, n);
    wipe(be, sizeof be);
}

static void sc_tobytes(proven_byte_t out[32], const proven_crypto_limb_t a[SC_LIMBS]) {
    for (int i = 0; i < 32; ++i) out[i] = (proven_byte_t)(a[i / PROVEN_CRYPTO_MP_LIMB_BYTES] >> (8 * (i % PROVEN_CRYPTO_MP_LIMB_BYTES)));
}

/* The secret scalar and the prefix from a seed (RFC 8032 section 5.1.5). */
static void ed_expand(proven_byte_t h[64], const proven_byte_t seed[32]) {
    proven_sha512((proven_mem_view_t){ .ptr = seed, .size = 32 }, h);
    h[0] &= 248; h[31] &= 127; h[31] |= 64;
}

void proven_crypto_ed25519_public(proven_byte_t pub[32], const proven_byte_t seed[32]) {
    proven_crypto_mp_mod_t l;
    proven_byte_t h[64];
    proven_crypto_limb_t a[SC_LIMBS];
    proven_ge_t p;
    (void)proven_crypto_mp_mod_init(&l, ED_L, sizeof ED_L);      /* a fixed odd modulus: cannot fail */
    ed_expand(h, seed);
    sc_reduce(&l, a, h, 32);                     /* the same point: B has order L */
    ge_mul_base(&p, a, l.n);
    ge_encode(pub, &p);
    PROVEN_CT_PUBLIC(pub, 32);
    wipe(h, sizeof h); wipe(a, sizeof a); wipe(&p, sizeof p);
}

void proven_crypto_ed25519_sign(proven_byte_t sig[64], const proven_byte_t seed[32], const proven_byte_t pub[32],
                                proven_mem_view_t msg) {
    proven_crypto_mp_mod_t l;
    proven_byte_t h[64], digest[64];
    proven_crypto_limb_t r[SC_LIMBS], k[SC_LIMBS], a[SC_LIMBS], s[SC_LIMBS];
    proven_sha512_t ctx;
    proven_ge_t rp;
    (void)proven_crypto_mp_mod_init(&l, ED_L, sizeof ED_L);      /* a fixed odd modulus: cannot fail */
    ed_expand(h, seed);
    /* r = H(prefix | M) mod L */
    proven_sha512_init(&ctx);
    proven_sha512_update(&ctx, (proven_mem_view_t){ .ptr = h + 32, .size = 32 });
    proven_sha512_update(&ctx, msg);
    proven_sha512_final(&ctx, digest);
    sc_reduce(&l, r, digest, 64);
    ge_mul_base(&rp, r, l.n);
    ge_encode(sig, &rp);
    PROVEN_CT_PUBLIC(sig, 32);                   /* R is half of the signature */
    /* k = H(R | A | M) mod L; S = r + k a mod L */
    proven_sha512_init(&ctx);
    proven_sha512_update(&ctx, (proven_mem_view_t){ .ptr = sig, .size = 32 });
    proven_sha512_update(&ctx, (proven_mem_view_t){ .ptr = pub, .size = 32 });
    proven_sha512_update(&ctx, msg);
    proven_sha512_final(&ctx, digest);
    sc_reduce(&l, k, digest, 64);
    sc_reduce(&l, a, h, 32);
    proven_crypto_mp_to_mont(&l, k, k);
    proven_crypto_mp_montmul(&l, s, k, a);
    proven_crypto_mp_add(&l, s, s, r);
    sc_tobytes(sig + 32, s);
    PROVEN_CT_PUBLIC(sig + 32, 32);
    wipe(h, sizeof h); wipe(digest, sizeof digest); wipe(r, sizeof r); wipe(a, sizeof a);
    wipe(s, sizeof s); wipe(&ctx, sizeof ctx); wipe(&rp, sizeof rp);
}

bool proven_crypto_ed25519_verify(const proven_byte_t pub[32], proven_mem_view_t msg, const proven_byte_t sig[64]) {
    proven_crypto_mp_mod_t l;
    proven_byte_t digest[64], sbe[32], kb[32], check[32];
    proven_crypto_limb_t s[SC_LIMBS], k[SC_LIMBS];
    proven_sha512_t ctx;
    proven_ge_t a, p;
    fe_t zero;
    if (!proven_crypto_mp_mod_init(&l, ED_L, sizeof ED_L)) return false;
    for (int i = 0; i < 32; ++i) sbe[i] = sig[63 - i];
    if (!proven_crypto_mp_load_be(s, SC_LIMBS, sbe, 32) || !proven_crypto_mp_lt(s, l.n, SC_LIMBS)) return false;
    if (!ge_decode(&a, pub)) return false;
    /* -A */
    fe_zero(zero);
    fe_sub(a.x, zero, a.x); fe_sub(a.t, zero, a.t);
    proven_sha512_init(&ctx);
    proven_sha512_update(&ctx, (proven_mem_view_t){ .ptr = sig, .size = 32 });
    proven_sha512_update(&ctx, (proven_mem_view_t){ .ptr = pub, .size = 32 });
    proven_sha512_update(&ctx, msg);
    proven_sha512_final(&ctx, digest);
    sc_reduce(&l, k, digest, 64);
    sc_tobytes(kb, k);
    ge_mul2_vartime(&p, s, kb, &a, l.n);
    /* [S]B - [k]A must encode to exactly the R that was sent: a non-canonical R cannot match. */
    ge_encode(check, &p);
    proven_byte_t diff = 0;
    for (int i = 0; i < 32; ++i) diff |= (proven_byte_t)(check[i] ^ sig[i]);
    return diff == 0;
}

/* ---- for the suite ---- */

/* Every entry of the base point's table against the general multiplication: entry i must be
 * m B for m = 2^(4d) + the sum over b < 4 of (+1 or -1, by bit b of i) 2^(b d). */
bool proven_crypto_ed25519_test_table(void) {
    proven_ge_t b, p;
    proven_byte_t m[32], got[32], want[32];
    ed_base(&b);
    for (proven_size_t i = 0; i < ED_GTAB_POINTS; ++i) {
        /* m = plus - minus, in bytes */
        proven_byte_t plus[32] = { 0 }, minus[32] = { 0 };
        plus[(4 * ED_COMB_D) / 8] |= (proven_byte_t)(1u << ((4 * ED_COMB_D) % 8));
        for (proven_size_t t = 0; t < 4; ++t) {
            proven_byte_t *to = ((i >> t) & 1u) ? plus : minus;
            to[(t * ED_COMB_D) / 8] |= (proven_byte_t)(1u << ((t * ED_COMB_D) % 8));
        }
        int borrow = 0;
        for (proven_size_t j = 0; j < 32; ++j) { int v = plus[j] - minus[j] - borrow; borrow = v < 0; m[j] = (proven_byte_t)v; }
        ge_mul(&p, m, &b);
        ge_encode(got, &p);
        /* the entry, encoded the same way: y with the sign of x in the top bit */
        for (proven_size_t j = 0; j < 32; ++j) want[j] = ED_GTAB[i][32 + j];
        want[31] |= (proven_byte_t)((ED_GTAB[i][0] & 1u) << 7);
        for (proven_size_t j = 0; j < 32; ++j) if (got[j] != want[j]) return false;
    }
    return true;
}

/* k B by the table and by the general walk, for k below L (32 bytes, little-endian; zero
 * too). True when the encodings agree. */
bool proven_crypto_ed25519_test_base_mul(const proven_byte_t k_le[32]) {
    proven_crypto_mp_mod_t l;
    proven_byte_t be[32], x[32], y[32];
    proven_crypto_limb_t k[SC_LIMBS];
    proven_ge_t b, p, q;
    if (!proven_crypto_mp_mod_init(&l, ED_L, sizeof ED_L)) return false;
    for (int i = 0; i < 32; ++i) be[i] = k_le[31 - i];
    if (!proven_crypto_mp_load_be(k, SC_LIMBS, be, 32) || !proven_crypto_mp_lt(k, l.n, SC_LIMBS)) return false;
    ed_base(&b);
    ge_mul(&p, k_le, &b);
    ge_mul_base(&q, k, l.n);
    ge_encode(x, &p); ge_encode(y, &q);
    for (int i = 0; i < 32; ++i) if (x[i] != y[i]) return false;
    return true;
}

/* s B + k Q by verification's walk and by the constant-time code, for s, k below L (32 bytes,
 * little-endian) and Q an encoded point. True when the encodings agree. */
bool proven_crypto_ed25519_test_mul2(const proven_byte_t s_le[32], const proven_byte_t k_le[32], const proven_byte_t q_enc[32]) {
    proven_crypto_mp_mod_t l;
    proven_byte_t be[32], x[32], y[32];
    proven_crypto_limb_t s[SC_LIMBS], k[SC_LIMBS];
    proven_ge_t b, q, p1, p2, v;
    if (!proven_crypto_mp_mod_init(&l, ED_L, sizeof ED_L) || !ge_decode(&q, q_enc)) return false;
    for (int i = 0; i < 32; ++i) be[i] = s_le[31 - i];
    if (!proven_crypto_mp_load_be(s, SC_LIMBS, be, 32) || !proven_crypto_mp_lt(s, l.n, SC_LIMBS)) return false;
    for (int i = 0; i < 32; ++i) be[i] = k_le[31 - i];
    if (!proven_crypto_mp_load_be(k, SC_LIMBS, be, 32) || !proven_crypto_mp_lt(k, l.n, SC_LIMBS)) return false;
    ed_base(&b);
    ge_mul(&p1, s_le, &b);
    ge_mul(&p2, k_le, &q);
    ge_add(&p1, &p1, &p2);
    ge_mul2_vartime(&v, s, k_le, &q, l.n);
    ge_encode(x, &p1); ge_encode(y, &v);
    for (int i = 0; i < 32; ++i) if (x[i] != y[i]) return false;
    return true;
}
