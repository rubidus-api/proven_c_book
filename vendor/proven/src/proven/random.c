#include "proven/random.h"

#ifndef PROVEN_FREESTANDING
#include "../../platform/proven_sys_random.h"
#endif

/*
 * Two generators, because there are two jobs (include/proven/random.h says which is which),
 * plus the helpers that make a draw uniform. Everything here is pure arithmetic: no OS, no
 * allocation, no libc. The only thing that talks to the operating system is the entropy
 * source at the bottom, and it is the only thing that can fail.
 *
 * Both algorithms are implemented from their specifications, not copied:
 *   - xoshiro256** and SplitMix64: Blackman & Vigna, released to the public domain.
 *   - ChaCha20: Bernstein, RFC 8439. Public domain.
 */

static inline proven_u64 rotl64(proven_u64 x, int k) {
    return (x << k) | (x >> (64 - k));
}

static inline proven_u32 rotl32(proven_u32 x, int k) {
    return (proven_u32)((x << k) | (x >> (32 - k)));
}

/* Zero a buffer so the compiler cannot elide it. A plain `for` loop that writes a buffer
 * nothing reads afterwards is a dead store, and the optimiser removes it - which is exactly
 * what happened to the seed scrub below: the loop was there, the comment was right about why,
 * and at -O1 it compiled to nothing. Writing through a volatile pointer is an observable side
 * effect the optimiser must keep, and unlike explicit_bzero/memset_s it needs no libc, so it
 * works freestanding too. */
static void secure_zero(void *buf, proven_size_t len) {
    volatile proven_byte_t *p = (volatile proven_byte_t *)buf;
    while (len--) *p++ = 0;
}

/* 64x64 -> 128. __int128 where the compiler has it, a portable 32-bit decomposition where it
 * does not - this has to work on i686 and on a Cortex-M as well as on x86-64. */
static inline void mul64x64(proven_u64 a, proven_u64 b, proven_u64 *hi, proven_u64 *lo) {
#if defined(__SIZEOF_INT128__)
    unsigned __int128 p = (unsigned __int128)a * (unsigned __int128)b;
    *lo = (proven_u64)p;
    *hi = (proven_u64)(p >> 64);
#else
    const proven_u64 mask = 0xFFFFFFFFu;
    proven_u64 a0 = a & mask, a1 = a >> 32;
    proven_u64 b0 = b & mask, b1 = b >> 32;
    proven_u64 p00 = a0 * b0;
    proven_u64 p01 = a0 * b1;
    proven_u64 p10 = a1 * b0;
    proven_u64 p11 = a1 * b1;
    proven_u64 mid = p10 + (p00 >> 32) + (p01 & mask);
    *lo = (mid << 32) | (p00 & mask);
    *hi = p11 + (mid >> 32) + (p01 >> 32);
#endif
}

// -------------------------------------------------------------
// SplitMix64 — used only to expand a seed
// -------------------------------------------------------------

/*
 * A 64-bit seed has to become 256 bits of xoshiro state, and it matters HOW. Writing the seed
 * (or a counter) straight into the state words gives a generator whose first outputs are
 * visibly correlated, and a seed of 0 gives the all-zero state, which xoshiro can never leave:
 * it emits zeros forever. SplitMix64 is a strong mixer with no fixed points that matter, so
 * every seed - including 0, 1, 2, the ones people actually pass - lands on a good state.
 */
static inline proven_u64 splitmix64(proven_u64 *x) {
    proven_u64 z = (*x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// -------------------------------------------------------------
// xoshiro256** — fast, reproducible, NOT secret-grade
// -------------------------------------------------------------

void proven_xoshiro256ss_seed(proven_xoshiro256ss_t *g, proven_u64 seed) {
    if (!g) return;
    proven_u64 x = seed;
    g->s[0] = splitmix64(&x);
    g->s[1] = splitmix64(&x);
    g->s[2] = splitmix64(&x);
    g->s[3] = splitmix64(&x);
}

proven_u64 proven_xoshiro256ss_next(proven_xoshiro256ss_t *g) {
    if (!g) return 0;
    const proven_u64 result = rotl64(g->s[1] * 5u, 7) * 9u;

    const proven_u64 t = g->s[1] << 17;
    g->s[2] ^= g->s[0];
    g->s[3] ^= g->s[1];
    g->s[1] ^= g->s[2];
    g->s[0] ^= g->s[3];
    g->s[2] ^= t;
    g->s[3] = rotl64(g->s[3], 45);

    return result;
}

static proven_u64 xoshiro_next_u64(void *ctx) {
    return proven_xoshiro256ss_next((proven_xoshiro256ss_t *)ctx);
}

static void xoshiro_fill(void *ctx, void *buf, proven_size_t len) {
    proven_xoshiro256ss_t *g = (proven_xoshiro256ss_t *)ctx;
    proven_byte_t *p = (proven_byte_t *)buf;
    while (len >= 8) {
        proven_u64 v = proven_xoshiro256ss_next(g);
        for (int i = 0; i < 8; ++i) p[i] = (proven_byte_t)(v >> (8 * i));
        p += 8;
        len -= 8;
    }
    if (len) {
        /* The tail. A fill whose length is not a multiple of 8 must still fill every byte. */
        proven_u64 v = proven_xoshiro256ss_next(g);
        for (proven_size_t i = 0; i < len; ++i) p[i] = (proven_byte_t)(v >> (8 * i));
    }
}

static const proven_rng_vtable_t g_xoshiro_vt = {
    .next_u64 = xoshiro_next_u64,
    .fill = xoshiro_fill,
};

proven_rng_t proven_xoshiro256ss_rng(proven_xoshiro256ss_t *g) {
    return (proven_rng_t){ .vt = &g_xoshiro_vt, .ctx = g };
}

// -------------------------------------------------------------
// ChaCha20 — cryptographic, and OS-free once seeded
// -------------------------------------------------------------

/* Set in `seeded` by seeding, and by nothing else. A zero-initialised generator - the shape of
 * "never seeded" - must not be usable, and `used` alone cannot say so: `used == 0` is exactly
 * what a zeroed struct holds, and the fill path read that as "a fresh block is ready", handing
 * the caller its own uninitialised block[]. */
#define CHACHA_SEEDED 0x5EEDEDu

#define CHACHA_QR(a, b, c, d)                        \
    do {                                             \
        a += b; d ^= a; d = rotl32(d, 16);           \
        c += d; b ^= c; b = rotl32(b, 12);           \
        a += b; d ^= a; d = rotl32(d, 8);            \
        c += d; b ^= c; b = rotl32(b, 7);            \
    } while (0)

/* One 64-byte keystream block from the current state, then step the counter. */
static void chacha_block(proven_chacha_rng_t *g) {
    proven_u32 x[16];
    for (int i = 0; i < 16; ++i) x[i] = g->state[i];

    /* 20 rounds = 10 double rounds: a column round then a diagonal round. */
    for (int i = 0; i < 10; ++i) {
        CHACHA_QR(x[0], x[4], x[8],  x[12]);
        CHACHA_QR(x[1], x[5], x[9],  x[13]);
        CHACHA_QR(x[2], x[6], x[10], x[14]);
        CHACHA_QR(x[3], x[7], x[11], x[15]);

        CHACHA_QR(x[0], x[5], x[10], x[15]);
        CHACHA_QR(x[1], x[6], x[11], x[12]);
        CHACHA_QR(x[2], x[7], x[8],  x[13]);
        CHACHA_QR(x[3], x[4], x[9],  x[14]);
    }

    /* Add the original state back in - this is what makes the block function non-invertible -
     * and serialise little-endian, as the standard specifies. */
    for (int i = 0; i < 16; ++i) {
        proven_u32 v = x[i] + g->state[i];
        g->block[4 * i + 0] = (proven_byte_t)(v      );
        g->block[4 * i + 1] = (proven_byte_t)(v >>  8);
        g->block[4 * i + 2] = (proven_byte_t)(v >> 16);
        g->block[4 * i + 3] = (proven_byte_t)(v >> 24);
    }
    g->used = 0;

    /* Step the 32-bit block counter. 2^32 blocks is 256 GiB of keystream from one seed; past
     * that the counter wraps and the stream would repeat, so carry into the first nonce word
     * rather than silently reusing keystream. */
    if (++g->state[12] == 0) {
        ++g->state[13];
    }
}

void proven_chacha_rng_seed(proven_chacha_rng_t *g, const proven_byte_t seed[PROVEN_CHACHA_SEED_SIZE]) {
    if (!g || !seed) return;

    /* "expand 32-byte k", as little-endian words. */
    g->state[0] = 0x61707865u;
    g->state[1] = 0x3320646Eu;
    g->state[2] = 0x79622D32u;
    g->state[3] = 0x6B206574u;

    for (int i = 0; i < 8; ++i) {
        g->state[4 + i] = (proven_u32)seed[4 * i + 0]
                        | ((proven_u32)seed[4 * i + 1] <<  8)
                        | ((proven_u32)seed[4 * i + 2] << 16)
                        | ((proven_u32)seed[4 * i + 3] << 24);
    }

    /* Counter 0, nonce 0. Every seed is a fresh key, so a fresh nonce buys nothing here - the
     * (key, nonce, counter) triple is unique because the key is. */
    g->state[12] = 0;
    g->state[13] = 0;
    g->state[14] = 0;
    g->state[15] = 0;

    g->used = 64;   /* nothing buffered: the next draw generates a block */
    g->seeded = CHACHA_SEEDED;
}

void proven_chacha_rng_fill(proven_chacha_rng_t *g, void *buf, proven_size_t len) {
    if (!g || !buf || len == 0) return;
    proven_byte_t *p = (proven_byte_t *)buf;

    /* Never seeded, or seeding failed. Hand back zeros - an obviously dead value - rather than
     * this frame's stack (which `used == 0` used to serve up as a "ready" block) or the fixed,
     * publicly derivable keystream of an all-zero key (which is what an advancing counter over
     * a zeroed state produces after the first block, and which passes every smell test). */
    if (g->seeded != CHACHA_SEEDED) {
        for (proven_size_t i = 0; i < len; ++i) p[i] = 0;
        return;
    }

    while (len > 0) {
        if (g->used >= 64) chacha_block(g);

        proven_size_t avail = 64 - g->used;
        proven_size_t take = (len < avail) ? len : avail;
        for (proven_size_t i = 0; i < take; ++i) p[i] = g->block[g->used + i];

        g->used += take;
        p += take;
        len -= take;
    }
}

proven_u64 proven_chacha_rng_next(proven_chacha_rng_t *g) {
    if (!g || g->seeded != CHACHA_SEEDED) return 0;

    proven_byte_t b[8];
    proven_chacha_rng_fill(g, b, sizeof b);
    proven_u64 v = 0;
    for (int i = 0; i < 8; ++i) v |= (proven_u64)b[i] << (8 * i);
    return v;
}

static proven_u64 chacha_next_u64(void *ctx) {
    return proven_chacha_rng_next((proven_chacha_rng_t *)ctx);
}

static void chacha_fill_ctx(void *ctx, void *buf, proven_size_t len) {
    proven_chacha_rng_fill((proven_chacha_rng_t *)ctx, buf, len);
}

static const proven_rng_vtable_t g_chacha_vt = {
    .next_u64 = chacha_next_u64,
    .fill = chacha_fill_ctx,
};

proven_rng_t proven_chacha_rng(proven_chacha_rng_t *g) {
    /* An unseeded generator is not a source of randomness, and must not claim to be one: every
     * helper - below, range, shuffle, f64 - draws from whatever it is handed. */
    if (!g || g->seeded != CHACHA_SEEDED) return (proven_rng_t){0};
    return (proven_rng_t){ .vt = &g_chacha_vt, .ctx = g };
}

// -------------------------------------------------------------
// The trait
// -------------------------------------------------------------

proven_u64 proven_rng_u64(proven_rng_t rng) {
    if (!proven_rng_is_valid(rng)) return 0;
    return rng.vt->next_u64(rng.ctx);
}

void proven_rng_fill(proven_rng_t rng, void *buf, proven_size_t len) {
    if (!proven_rng_is_valid(rng) || !buf || len == 0) return;
    rng.vt->fill(rng.ctx, buf, len);
}

// -------------------------------------------------------------
// Uniform helpers
// -------------------------------------------------------------

proven_u64 proven_rng_below(proven_rng_t rng, proven_u64 bound) {
    if (bound == 0 || !proven_rng_is_valid(rng)) return 0;

    /*
     * Lemire's multiply-and-reject. `x % bound` is what everyone writes: it is biased whenever
     * bound does not divide 2^64, because the 2^64 outcomes cannot be split into `bound` equal
     * groups, and the remainder favours the low values.
     *
     * Instead: multiply a random 64-bit x by bound, giving a 128-bit product. The high half is
     * uniform over [0, bound) EXCEPT that the first (2^64 mod bound) values of x each get one
     * extra chance. Reject exactly those - detectable as `lo < threshold` - and the rest is
     * exactly uniform. The rejection zone is smaller than bound/2^64, so for any realistic
     * bound the loop runs once.
     */
    proven_u64 hi, lo;
    mul64x64(rng.vt->next_u64(rng.ctx), bound, &hi, &lo);

    if (lo < bound) {
        /* threshold = 2^64 mod bound, computed without a 128-bit divide. */
        proven_u64 threshold = (proven_u64)(0u - bound) % bound;
        while (lo < threshold) {
            mul64x64(rng.vt->next_u64(rng.ctx), bound, &hi, &lo);
        }
    }
    return hi;
}

proven_i64 proven_rng_range(proven_rng_t rng, proven_i64 lo, proven_i64 hi) {
    if (hi < lo) return lo;

    /* The span in UNSIGNED arithmetic, so INT64_MIN..INT64_MAX (a span of 2^64-1) does not
     * overflow the way (hi - lo) would. */
    proven_u64 span = (proven_u64)hi - (proven_u64)lo;

    if (span == (proven_u64)~0ull) {
        /* The whole 64-bit range: every value is valid, so no rejection is needed. */
        return (proven_i64)proven_rng_u64(rng);
    }

    proven_u64 r = proven_rng_below(rng, span + 1u);
    return (proven_i64)((proven_u64)lo + r);
}

double proven_rng_f64(proven_rng_t rng) {
    /* 53 bits - a double's whole mantissa - scaled by 2^-53. Every representable value in
     * [0,1) is reachable and equally likely, and 1.0 is not: the largest draw is
     * (2^53 - 1) / 2^53. */
    proven_u64 v = proven_rng_u64(rng) >> 11;
    return (double)v * (1.0 / 9007199254740992.0);   /* 2^53 */
}

void proven_rng_shuffle(proven_rng_t rng, void *base, proven_size_t count, proven_size_t elem_size) {
    if (!base || elem_size == 0 || count < 2 || !proven_rng_is_valid(rng)) return;

    proven_byte_t *a = (proven_byte_t *)base;

    /* Fisher-Yates, backwards: element i is swapped with a uniformly chosen element in [0, i].
     * The index comes from proven_rng_below, so the permutation is unbiased - the `% n` version
     * of this loop measurably favours some orderings. */
    for (proven_size_t i = count - 1; i > 0; --i) {
        proven_u64 j = proven_rng_below(rng, (proven_u64)i + 1u);
        if ((proven_size_t)j == i) continue;

        proven_byte_t *x = a + i * elem_size;
        proven_byte_t *y = a + (proven_size_t)j * elem_size;
        for (proven_size_t b = 0; b < elem_size; ++b) {
            proven_byte_t t = x[b];
            x[b] = y[b];
            y[b] = t;
        }
    }
}

// -------------------------------------------------------------
// The entropy source - the only thing here that can fail
// -------------------------------------------------------------

/*
 * Entropy is the one thing a program cannot compute for itself, so it is the one thing that
 * has to come from outside - and the outside is not always an operating system. A board has
 * real entropy (a TRNG, a ring oscillator, an ADC's noise floor) and no getrandom(); a hosted
 * process has getrandom() and no idea what a ring oscillator is. Both feed the same generators,
 * so the source is a hook rather than a hard-coded call.
 *
 * The hook is a plain global, deliberately unsynchronised: it is installed once at startup,
 * before any thread asks for a key. A mutex here would not make "swap the entropy source while
 * another thread is deriving a key" safe - it would only make it quiet.
 */
static proven_entropy_fn g_entropy_fn = NULL;
static void *g_entropy_ctx = NULL;

void proven_random_set_source(proven_entropy_fn fn, void *ctx) {
    g_entropy_fn = fn;
    g_entropy_ctx = ctx;
}

bool proven_random_bytes(void *buf, proven_size_t len) {
    if (len == 0) return true;
    if (!buf) return false;

    if (g_entropy_fn) return g_entropy_fn(g_entropy_ctx, buf, len);

#ifndef PROVEN_FREESTANDING
    /* The platform default: the OS CSPRNG. A hosted caller never has to install anything. */
    return proven_sys_random_bytes(buf, len);
#else
    /* A bare-metal target with no source installed. There is nothing to fall back to, and the
     * honest answer is to say so: a clock-seeded PRNG here would look like success and be a
     * security hole nothing reports. */
    return false;
#endif
}

proven_u64 proven_random_u64(void) {
    proven_u64 v = 0;
    if (!proven_random_bytes(&v, sizeof v)) return 0;
    return v;
}

bool proven_chacha_rng_seed_from_entropy(proven_chacha_rng_t *g) {
    if (!g) return false;

    proven_byte_t seed[PROVEN_CHACHA_SEED_SIZE];
    if (!proven_random_bytes(seed, sizeof seed)) {
        /* Leave the generator unusable rather than seeded with whatever was on the stack - and
         * unusable FOREVER, which zeroing the state alone did not achieve. ChaCha over an
         * all-zero state emits an all-zero first block, so "the caller gets zeros" was true for
         * exactly 64 bytes; then the counter advanced and block 1 was a normal-looking, fixed,
         * publicly derivable keystream. Clearing the marker is what actually stops it. */
        for (int i = 0; i < 16; ++i) g->state[i] = 0;
        g->used = 64;
        g->seeded = 0;
        return false;
    }

    proven_chacha_rng_seed(g, seed);

    /* Do not leave the seed lying in this frame - and do it in a way the optimiser cannot
     * throw away, because a plain loop here IS thrown away (the seed is never read again). */
    secure_zero(seed, sizeof seed);
    return true;
}
