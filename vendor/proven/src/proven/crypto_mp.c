#include "proven_internal_crypto.h"

/* Multi-precision modular arithmetic in Montgomery form. A limb is 64 bits where the compiler
 * has a 128-bit integer for the product of two and the target is a 64-bit one, and 32 bits
 * elsewhere, so that a product is one machine multiplication on every target. Loops run over
 * the modulus size only; selections are made with masks. The 128-bit type is multiplied,
 * added and shifted here and never divided: a division of it would be a call into the
 * compiler's runtime, which a freestanding build does not have. */

typedef proven_crypto_limb_t proven_limb_t;
#if PROVEN_CRYPTO_MP_LIMB_BITS == 64
__extension__ typedef unsigned __int128 wide_t;
#else
typedef proven_u64 wide_t;
#endif
#define W PROVEN_CRYPTO_MP_LIMB_BITS
/* The inner loops of the products, four limbs a turn: the loop's own counting was a quarter of
 * the squaring's instructions (RFC-0028 S-4). Only a hint; the work is the same. */
#if defined(__clang__)
#define PROVEN_MP_UNROLL _Pragma("clang loop unroll_count(4)")
#elif defined(__GNUC__)
#define PROVEN_MP_UNROLL _Pragma("GCC unroll 4")
#else
#define PROVEN_MP_UNROLL
#endif
#define LB PROVEN_CRYPTO_MP_LIMB_BYTES

static proven_limb_t mp_mask_from_bit(proven_limb_t bit) { return (proven_limb_t)0 - (bit & 1u); }
/* All ones when x is zero. */
static proven_limb_t mp_mask_zero(proven_limb_t x) { return ((x | ((proven_limb_t)0 - x)) >> (W - 1)) - 1u; }

bool proven_crypto_mp_load_be(proven_limb_t *out, proven_size_t limbs, const proven_byte_t *be, proven_size_t len) {
    for (proven_size_t i = 0; i < limbs; ++i) out[i] = 0;
    for (proven_size_t i = 0; i < len; ++i) {
        proven_byte_t v = be[len - 1 - i];
        if (i / LB >= limbs) {
            if (v != 0) return false;
            continue;
        }
        out[i / LB] |= (proven_limb_t)v << (8 * (i % LB));
    }
    return true;
}

void proven_crypto_mp_store_be(proven_byte_t *out, proven_size_t len, const proven_limb_t *a, proven_size_t limbs) {
    for (proven_size_t i = 0; i < len; ++i) {
        proven_size_t limb = i / LB;
        out[len - 1 - i] = limb < limbs ? (proven_byte_t)(a[limb] >> (8 * (i % LB))) : 0;
    }
}

proven_limb_t proven_crypto_mp_sub_raw(proven_limb_t *out, const proven_limb_t *a, const proven_limb_t *b, proven_size_t limbs) {
    wide_t borrow = 0;
    for (proven_size_t i = 0; i < limbs; ++i) {
        wide_t d = (wide_t)a[i] - b[i] - borrow;
        out[i] = (proven_limb_t)d;
        borrow = (d >> W) & 1u;
    }
    return (proven_limb_t)borrow;
}

proven_limb_t proven_crypto_mp_add_raw(proven_limb_t *out, const proven_limb_t *a, const proven_limb_t *b, proven_size_t limbs) {
    wide_t carry = 0;
    for (proven_size_t i = 0; i < limbs; ++i) {
        wide_t s = (wide_t)a[i] + b[i] + carry;
        out[i] = (proven_limb_t)s;
        carry = s >> W;
    }
    return (proven_limb_t)carry;
}

proven_limb_t proven_crypto_mp_lt(const proven_limb_t *a, const proven_limb_t *b, proven_size_t limbs) {
    wide_t borrow = 0;
    for (proven_size_t i = 0; i < limbs; ++i) {
        wide_t d = (wide_t)a[i] - b[i] - borrow;
        borrow = (d >> W) & 1u;
    }
    return mp_mask_from_bit((proven_limb_t)borrow);
}

proven_limb_t proven_crypto_mp_is_zero(const proven_limb_t *a, proven_size_t limbs) {
    proven_limb_t acc = 0;
    for (proven_size_t i = 0; i < limbs; ++i) acc |= a[i];
    return mp_mask_zero(acc);
}

proven_limb_t proven_crypto_mp_eq(const proven_limb_t *a, const proven_limb_t *b, proven_size_t limbs) {
    proven_limb_t acc = 0;
    for (proven_size_t i = 0; i < limbs; ++i) acc |= a[i] ^ b[i];
    return mp_mask_zero(acc);
}

void proven_crypto_mp_select(proven_limb_t *out, const proven_limb_t *a, const proven_limb_t *b, proven_limb_t mask, proven_size_t limbs) {
    for (proven_size_t i = 0; i < limbs; ++i) out[i] = (a[i] & mask) | (b[i] & ~mask);
}

static void mp_mod_finish(proven_crypto_mp_mod_t *mod, bool top_limb_set);

bool proven_crypto_mp_mod_init_secret(proven_crypto_mp_mod_t *mod, const proven_byte_t *be, proven_size_t len) {
    /* Nothing here may depend on the value: no stripping of leading zeros, no count of
     * leading bits. One bit of it is examined - that it is odd - and odd is what every prime
     * factor of an RSA modulus is. */
    if (len < 8 || len > PROVEN_CRYPTO_MP_MAX_BYTES) return false;
    proven_size_t limbs = PROVEN_CRYPTO_MP_LIMBS(len);
    for (proven_size_t i = 0; i < limbs; ++i) mod->n[i] = 0;
    for (proven_size_t i = 0; i < len; ++i) mod->n[i / LB] |= (proven_limb_t)be[len - 1 - i] << (8 * (i % LB));
    proven_limb_t odd = mod->n[0] & 1u;
    PROVEN_CT_PUBLIC(&odd, sizeof odd);
    if (!odd) return false;
    mod->limbs = limbs;
    mod->bits = len * 8;                          /* an upper bound: the true length is not looked for */
    mp_mod_finish(mod, false);
    return true;
}

bool proven_crypto_mp_mod_init(proven_crypto_mp_mod_t *mod, const proven_byte_t *be, proven_size_t len) {
    while (len > 0 && be[0] == 0) { be++; len--; }
    if (len == 0 || len > PROVEN_CRYPTO_MP_MAX_BYTES) return false;
    proven_size_t limbs = PROVEN_CRYPTO_MP_LIMBS(len);
    if (!proven_crypto_mp_load_be(mod->n, limbs, be, len)) return false;
    if ((mod->n[0] & 1u) == 0) return false;
    if (limbs == 1 && mod->n[0] < 3) return false;
    mod->limbs = limbs;
    mod->bits = len * 8;
    for (proven_byte_t top = be[0]; (top & 0x80u) == 0; top = (proven_byte_t)(top << 1)) mod->bits--;
    mp_mod_finish(mod, true);                     /* its leading zero bytes are gone: the top limb is not zero */
    return true;
}

/* n0inv and R^2 from the modulus's limbs: arithmetic only, and the same steps for every
 * modulus of one size. `top_limb_set` is PUBLIC: it says the modulus's top limb is not zero,
 * which a modulus that was stripped of its leading zeros can say and a secret one cannot. */
static void mp_mod_finish(proven_crypto_mp_mod_t *mod, bool top_limb_set) {
    const proven_size_t limbs = mod->limbs;
    /* Newton's iteration doubles the correct low bits each round: 5 rounds from 3 bits is 96. */
    proven_limb_t inv = mod->n[0];
    for (int i = 0; i < 5; ++i) inv *= 2u - mod->n[0] * inv;
    mod->n0inv = (proven_limb_t)0 - inv;
    /* 2^W * R mod n by doubling, subtracting n whenever the value reaches it. From 1 that is
     * W * (limbs + 1) doublings; from 2^(W * (limbs - 1)), which is below a modulus whose
     * top limb is not zero, it is 2 W of them. */
    proven_limb_t *r = mod->rr;
    proven_limb_t t[PROVEN_CRYPTO_MP_MAX], c[PROVEN_CRYPTO_MP_MAX];
    for (proven_size_t i = 0; i < limbs; ++i) r[i] = 0;
    r[top_limb_set ? limbs - 1 : 0] = 1;
    const proven_size_t doublings = top_limb_set ? 2 * (proven_size_t)W : (proven_size_t)W * (limbs + 1);
    for (proven_size_t k = 0; k < doublings; ++k) {
        proven_limb_t carry = proven_crypto_mp_add_raw(r, r, r, limbs);
        proven_limb_t borrow = proven_crypto_mp_sub_raw(t, r, mod->n, limbs);
        /* Keep the difference when the doubling overflowed or did not borrow. */
        proven_crypto_mp_select(r, t, r, mp_mask_from_bit(carry | (borrow ^ 1u)), limbs);
    }
    /* That is 2^W in Montgomery form, and R is (2^W)^limbs: its `limbs`-th power, by squaring
     * and multiplying over the bits of the limb count - a dozen multiplications where
     * doubling all the way would be W * limbs more passes. The count is public. */
    for (proven_size_t i = 0; i < limbs; ++i) c[i] = r[i];
    int top = 0;
    for (int bit = 0; bit < 16; ++bit) if ((limbs >> bit) & 1u) top = bit;
    for (int bit = top - 1; bit >= 0; --bit) {
        proven_crypto_mp_montsqr(mod, r, r);
        if ((limbs >> bit) & 1u) proven_crypto_mp_montmul(mod, r, r, c);
    }
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)t, .size = sizeof t });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)c, .size = sizeof c });
}

void proven_crypto_mp_montmul(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *a, const proven_limb_t *b) {
    const proven_size_t s = mod->limbs;
    const proven_limb_t *n = mod->n;
    const proven_limb_t n0inv = mod->n0inv;
    /* t stays below 2n: s limbs and one bit in t[s]. For each limb of b, t + a b_i + m n,
     * shifted down a limb, in one pass: m is known from the lowest limb before the pass, so
     * the product and the reduction go side by side, two carries kept. */
    proven_limb_t t[PROVEN_CRYPTO_MP_MAX + 1];
    for (proven_size_t i = 0; i <= s; ++i) t[i] = 0;
    for (proven_size_t i = 0; i < s; ++i) {
        const wide_t bi = b[i];
        wide_t v = (wide_t)a[0] * bi + t[0];
        wide_t c1 = v >> W;
        const wide_t m = (proven_limb_t)((proven_limb_t)v * n0inv);
        wide_t c2 = (m * n[0] + (proven_limb_t)v) >> W;
        PROVEN_MP_UNROLL
        for (proven_size_t j = 1; j < s; ++j) {
            v = (wide_t)a[j] * bi + t[j] + c1;
            c1 = v >> W;
            const wide_t u = m * n[j] + (proven_limb_t)v + c2;
            t[j - 1] = (proven_limb_t)u;
            c2 = u >> W;
        }
        v = (wide_t)t[s] + c1 + c2;
        t[s - 1] = (proven_limb_t)v;
        t[s] = (proven_limb_t)(v >> W);
    }
    /* The result is below 2n: one subtraction, chosen by mask. */
    proven_limb_t d[PROVEN_CRYPTO_MP_MAX];
    proven_limb_t borrow = proven_crypto_mp_sub_raw(d, t, n, s);
    proven_crypto_mp_select(out, d, t, mp_mask_from_bit(t[s] | (borrow ^ 1u)), s);
}

/* out = a * a / R mod n: the square of a Montgomery-form value. Each product of two different
 * limbs is computed once and doubled, so the square takes about half the multiplications of
 * montmul's; then the reduction, a limb at a time, as montmul's. The same loops for every
 * value: nothing here depends on a's digits but the arithmetic. */
void proven_crypto_mp_montsqr(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *a) {
    const proven_size_t s = mod->limbs;
    const proven_limb_t *n = mod->n;
    proven_limb_t t[2 * PROVEN_CRYPTO_MP_MAX];
    for (proven_size_t i = 0; i < 2 * s; ++i) t[i] = 0;
    /* The products a_i a_j for i < j, each once. */
    for (proven_size_t i = 0; i + 1 < s; ++i) {
        wide_t c = 0;
        const wide_t ai = a[i];
        PROVEN_MP_UNROLL
        for (proven_size_t j = i + 1; j < s; ++j) {
            const wide_t v = ai * a[j] + t[i + j] + c;
            t[i + j] = (proven_limb_t)v;
            c = v >> W;
        }
        t[i + s] = (proven_limb_t)c;
    }
    /* Doubled, then the squares a_i^2 added on the diagonal. */
    t[2 * s - 1] = t[2 * s - 2] >> (W - 1);
    for (proven_size_t k = 2 * s - 2; k > 0; --k) t[k] = (t[k] << 1) | (t[k - 1] >> (W - 1));
    t[0] <<= 1;
    wide_t c = 0;
    for (proven_size_t i = 0; i < s; ++i) {
        const wide_t sq = (wide_t)a[i] * a[i];
        wide_t v = (wide_t)t[2 * i] + (proven_limb_t)sq + c;
        t[2 * i] = (proven_limb_t)v;
        v = (wide_t)t[2 * i + 1] + (proven_limb_t)(sq >> W) + (v >> W);
        t[2 * i + 1] = (proven_limb_t)v;
        c = v >> W;
    }
    /* The reduction: m n added at each of the low s limbs makes it zero; what carries past the
     * top is kept in `top` and added at the next position. */
    proven_limb_t top = 0;
    for (proven_size_t i = 0; i < s; ++i) {
        const wide_t m = (proven_limb_t)(t[i] * mod->n0inv);
        wide_t cc = 0;
        PROVEN_MP_UNROLL
        for (proven_size_t j = 0; j < s; ++j) {
            const wide_t v = m * n[j] + t[i + j] + cc;
            t[i + j] = (proven_limb_t)v;
            cc = v >> W;
        }
        const wide_t v = (wide_t)t[i + s] + cc + top;
        t[i + s] = (proven_limb_t)v;
        top = (proven_limb_t)(v >> W);
    }
    /* t[s..2s) plus top is below 2n: one subtraction, chosen by mask. */
    proven_limb_t d[PROVEN_CRYPTO_MP_MAX];
    const proven_limb_t borrow = proven_crypto_mp_sub_raw(d, t + s, n, s);
    proven_crypto_mp_select(out, d, t + s, mp_mask_from_bit(top | (borrow ^ 1u)), s);
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)t, .size = 2 * s * sizeof t[0] });     /* the part used: the whole is 2 KiB */
}

void proven_crypto_mp_to_mont(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *a) {
    proven_crypto_mp_montmul(mod, out, a, mod->rr);
}

void proven_crypto_mp_from_mont(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *a) {
    proven_limb_t one[PROVEN_CRYPTO_MP_MAX];
    for (proven_size_t i = 0; i < mod->limbs; ++i) one[i] = 0;
    one[0] = 1;
    proven_crypto_mp_montmul(mod, out, a, one);
}

void proven_crypto_mp_add(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *a, const proven_limb_t *b) {
    proven_limb_t s[PROVEN_CRYPTO_MP_MAX], d[PROVEN_CRYPTO_MP_MAX];
    proven_limb_t carry = proven_crypto_mp_add_raw(s, a, b, mod->limbs);
    proven_limb_t borrow = proven_crypto_mp_sub_raw(d, s, mod->n, mod->limbs);
    proven_crypto_mp_select(out, d, s, mp_mask_from_bit(carry | (borrow ^ 1u)), mod->limbs);
}

void proven_crypto_mp_sub(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *a, const proven_limb_t *b) {
    proven_limb_t d[PROVEN_CRYPTO_MP_MAX], s[PROVEN_CRYPTO_MP_MAX];
    proven_limb_t borrow = proven_crypto_mp_sub_raw(d, a, b, mod->limbs);
    (void)proven_crypto_mp_add_raw(s, d, mod->n, mod->limbs);
    proven_crypto_mp_select(out, s, d, mp_mask_from_bit(borrow), mod->limbs);
}

void proven_crypto_mp_reduce_be(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_byte_t *be, proven_size_t len) {
    /* A chunk of the modulus's size at a time, from the top: r = r R + chunk (mod n), R being
     * 2^(W s). r R is to_mont(r), r < n; a chunk below R comes below n as
     * from_mont(to_mont(chunk)). Both are right because a Montgomery product is below 2n -
     * one subtraction from right - whenever a b < R n, which holds for a below R and b below
     * n (as R^2 mod n is). The first chunk is the short one, so the rest are whole. */
    const proven_size_t s = mod->limbs, chunk = s * LB;
    proven_limb_t r[PROVEN_CRYPTO_MP_MAX], c[PROVEN_CRYPTO_MP_MAX];
    for (proven_size_t i = 0; i < s; ++i) r[i] = 0;
    for (proven_size_t pos = 0; pos < len;) {
        const proven_size_t take = pos == 0 && len % chunk != 0 ? len % chunk : chunk;
        (void)proven_crypto_mp_load_be(c, s, be + pos, take);    /* take <= chunk: it fits */
        proven_crypto_mp_to_mont(mod, c, c);
        proven_crypto_mp_from_mont(mod, c, c);
        proven_crypto_mp_to_mont(mod, r, r);
        proven_crypto_mp_add(mod, r, r, c);
        pos += take;
    }
    for (proven_size_t i = 0; i < s; ++i) out[i] = r[i];
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)c, .size = s * sizeof c[0] });
}

/* A modulus of up to this many limbs - the curves' - and an exponent longer than 64 bits
 * (an inversion's) take the exponent four bits at a time below: a table of eight odd powers,
 * small at this size, for about a fifth as many multiplications as squarings. */
#define MP_POW_WINDOW_LIMBS PROVEN_CRYPTO_MP_LIMBS(48)

void proven_crypto_mp_pow(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *base,
                          const proven_limb_t *exp, proven_size_t exp_limbs) {
    const proven_size_t s = mod->limbs;
    proven_limb_t acc[PROVEN_CRYPTO_MP_MAX], b[PROVEN_CRYPTO_MP_MAX], one[PROVEN_CRYPTO_MP_MAX];
    for (proven_size_t i = 0; i < s; ++i) { one[i] = 0; b[i] = base[i]; }
    one[0] = 1;
    proven_crypto_mp_to_mont(mod, acc, one);
    /* The exponent is public, and so is where its bits begin: nothing is squared before the first. */
    proven_size_t top = 0;                                   /* one past the highest set bit */
    for (proven_size_t i = 0; i < exp_limbs * W; ++i) if ((exp[i / W] >> (i % W)) & 1u) top = i + 1;
    if (s <= MP_POW_WINDOW_LIMBS && top > 64) {
        /* Sliding windows: odd[j] = base^(2j + 1). A window starts at a set bit, is at most
         * four bits long and ends at a set bit, so its value is odd. */
        proven_limb_t odd[8][MP_POW_WINDOW_LIMBS], sq[MP_POW_WINDOW_LIMBS];
        for (proven_size_t i = 0; i < s; ++i) odd[0][i] = b[i];
        proven_crypto_mp_montsqr(mod, sq, b);
        for (int j = 1; j < 8; ++j) proven_crypto_mp_montmul(mod, odd[j], odd[j - 1], sq);
        bool started = false;
        for (proven_size_t i = top; i-- > 0;) {
            if (((exp[i / W] >> (i % W)) & 1u) == 0) { proven_crypto_mp_montsqr(mod, acc, acc); continue; }
            proven_size_t low = i >= 3 ? i - 3 : 0;
            while (((exp[low / W] >> (low % W)) & 1u) == 0) low++;
            proven_limb_t v = 0;
            for (proven_size_t k = i + 1; k-- > low;) v = (v << 1) | ((exp[k / W] >> (k % W)) & 1u);
            if (started) { for (proven_size_t k = low; k <= i; ++k) proven_crypto_mp_montsqr(mod, acc, acc); }
            proven_crypto_mp_montmul(mod, acc, acc, odd[v >> 1]);
            started = true;
            i = low;
        }
        for (proven_size_t i = 0; i < s; ++i) out[i] = acc[i];
        return;
    }
    bool started = false;
    for (proven_size_t i = top; i-- > 0;) {
        const bool set = ((exp[i / W] >> (i % W)) & 1u) != 0;
        if (started) proven_crypto_mp_montsqr(mod, acc, acc);
        if (set) { proven_crypto_mp_montmul(mod, acc, acc, b); started = true; }
    }
    for (proven_size_t i = 0; i < s; ++i) out[i] = acc[i];
}

bool proven_crypto_mp_pow_ct(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *base, const proven_limb_t *exp) {
    const proven_size_t s = mod->limbs;
    if (s > PROVEN_CRYPTO_MP_CT_MAX) return false;
    proven_limb_t table[16][PROVEN_CRYPTO_MP_CT_MAX], sel[PROVEN_CRYPTO_MP_CT_MAX], acc[PROVEN_CRYPTO_MP_CT_MAX];
    /* table[k] = base^k, in Montgomery form. */
    for (proven_size_t j = 0; j < s; ++j) { acc[j] = 0; table[1][j] = base[j]; }
    acc[0] = 1;
    proven_crypto_mp_to_mont(mod, table[0], acc);
    for (int k = 2; k < 16; ++k) proven_crypto_mp_montmul(mod, table[k], table[k - 1], base);
    for (proven_size_t j = 0; j < s; ++j) acc[j] = table[0][j];
    /* Every window does the same work: four squarings, a pass over the whole table that keeps
     * the wanted entry by mask, one multiplication - by 1 when the window is zero. */
    for (proven_size_t i = s; i-- > 0;) {
        for (int nibble = W / 4 - 1; nibble >= 0; --nibble) {
            for (int q = 0; q < 4; ++q) proven_crypto_mp_montsqr(mod, acc, acc);
            const proven_limb_t w = (exp[i] >> (4 * nibble)) & 15u;
            for (proven_size_t j = 0; j < s; ++j) sel[j] = 0;
            for (proven_limb_t k = 0; k < 16; ++k) {
                const proven_limb_t mask = mp_mask_zero(k ^ w);                 /* all ones when k == w */
                for (proven_size_t j = 0; j < s; ++j) sel[j] |= table[k][j] & mask;
            }
            proven_crypto_mp_montmul(mod, acc, acc, sel);
        }
    }
    for (proven_size_t j = 0; j < s; ++j) out[j] = acc[j];
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)table, .size = sizeof table });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)sel, .size = sizeof sel });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)acc, .size = sizeof acc });
    return true;
}

/* The same walk for a modulus of up to PROVEN_CRYPTO_MP_DH_MAX limbs and an exponent of
 * `exp_limbs` limbs - a count that is public, as a Diffie-Hellman exponent's length is. */
bool proven_crypto_mp_pow_ct_short(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *base, const proven_limb_t *exp, proven_size_t exp_limbs) {
    const proven_size_t s = mod->limbs;
    if (s > PROVEN_CRYPTO_MP_DH_MAX || exp_limbs == 0) return false;
    /* One flat table of 4 KiB serves both sizes: sixteen powers for a modulus of up to
     * 2048 bits (windows of four bits), four for a larger one (windows of two). That halves
     * the stack this needs, at the price of some multiplications for the larger groups. */
    const unsigned wbits = s * LB <= 256 ? 4u : 2u, entries = 1u << wbits;
    proven_limb_t table[4096 / LB], sel[PROVEN_CRYPTO_MP_DH_MAX], acc[PROVEN_CRYPTO_MP_DH_MAX];
    for (proven_size_t j = 0; j < s; ++j) { acc[j] = 0; table[s + j] = base[j]; }
    acc[0] = 1;
    proven_crypto_mp_to_mont(mod, table, acc);
    for (unsigned k = 2; k < entries; ++k) proven_crypto_mp_montmul(mod, table + k * s, table + (k - 1) * s, base);
    for (proven_size_t j = 0; j < s; ++j) acc[j] = table[j];
    for (proven_size_t i = exp_limbs; i-- > 0;) {
        for (int shift = W - (int)wbits; shift >= 0; shift -= (int)wbits) {
            for (unsigned q = 0; q < wbits; ++q) proven_crypto_mp_montsqr(mod, acc, acc);
            const proven_limb_t w = (exp[i] >> shift) & (entries - 1);
            for (proven_size_t j = 0; j < s; ++j) sel[j] = 0;
            for (proven_limb_t k = 0; k < entries; ++k) {
                const proven_limb_t mask = mp_mask_zero(k ^ w);
                for (proven_size_t j = 0; j < s; ++j) sel[j] |= table[k * s + j] & mask;
            }
            proven_crypto_mp_montmul(mod, acc, acc, sel);
        }
    }
    for (proven_size_t j = 0; j < s; ++j) out[j] = acc[j];
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)table, .size = sizeof table });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)sel, .size = sizeof sel });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)acc, .size = sizeof acc });
    return true;
}

void proven_crypto_mp_mul(proven_limb_t *out, const proven_limb_t *a, proven_size_t alen, const proven_limb_t *b, proven_size_t blen) {
    for (proven_size_t i = 0; i < alen + blen; ++i) out[i] = 0;
    for (proven_size_t i = 0; i < alen; ++i) {
        wide_t c = 0;
        const wide_t ai = a[i];
        for (proven_size_t j = 0; j < blen; ++j) {
            wide_t v = ai * b[j] + out[i + j] + c;
            out[i + j] = (proven_limb_t)v;
            c = v >> W;
        }
        out[i + blen] = (proven_limb_t)c;
    }
}

void proven_crypto_mp_inv_prime(const proven_crypto_mp_mod_t *mod, proven_limb_t *out, const proven_limb_t *a) {
    /* a^(n-2). The exponent is the public modulus; the base may be secret. */
    proven_limb_t e[PROVEN_CRYPTO_MP_MAX], two[PROVEN_CRYPTO_MP_MAX];
    for (proven_size_t i = 0; i < mod->limbs; ++i) two[i] = 0;
    two[0] = 2;
    (void)proven_crypto_mp_sub_raw(e, mod->n, two, mod->limbs);
    proven_crypto_mp_pow(mod, out, a, e, mod->limbs);
}

proven_u32 proven_crypto_mp_mod_small(const proven_limb_t *a, proven_size_t limbs, proven_u32 d) {
    /* Half a limb at a time where a limb is 64 bits, so that what is divided is a 64-bit word. */
    proven_u64 r = 0;
    for (proven_size_t i = limbs; i-- > 0;) {
        for (int shift = W - 32; shift >= 0; shift -= 32) r = ((r << 32) | (proven_u32)(a[i] >> shift)) % d;
    }
    return (proven_u32)r;
}

void proven_crypto_mp_inverse_from(proven_limb_t *out, const proven_limb_t *a, proven_size_t limbs, proven_u32 k, proven_u32 e) {
    wide_t carry = 1;
    for (proven_size_t i = 0; i < limbs; ++i) {
        wide_t v = (wide_t)a[i] * k + carry;
        out[i] = (proven_limb_t)v;
        carry = v >> W;
    }
    out[limbs] = (proven_limb_t)carry;
    proven_u64 r = 0;
    for (proven_size_t i = limbs + 1; i-- > 0;) {
        proven_limb_t q = 0;
        for (int shift = W - 32; shift >= 0; shift -= 32) {
            proven_u64 v = (r << 32) | (proven_u32)(out[i] >> shift);
            q |= (proven_limb_t)(v / e) << shift;
            r = v % e;
        }
        out[i] = q;
    }
}
