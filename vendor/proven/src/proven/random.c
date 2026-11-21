#include "proven/random.h"

#ifndef PROVEN_FREESTANDING
#include "../../platform/proven_sys_random.h"
#endif
#include <stdatomic.h>

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
#if (defined(__GNUC__) || defined(__clang__)) && !defined(PROVEN_FREESTANDING)
    /* Wide stores, then a barrier that says the memory is read: the stores cannot be dropped.
     * Byte-wide volatile stores cost a key-erasing generator (RFC-0048) more than its keystream
     * for small draws. */
    __builtin_memset(buf, 0, len);
    __asm__ __volatile__("" : : "r"(buf) : "memory");
#else
    volatile proven_byte_t *p = (volatile proven_byte_t *)buf;
    while (len--) *p++ = 0;
#endif
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
// SplitMix64 - used only to expand a seed
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
// xoshiro256** - fast, reproducible, NOT secret-grade
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
// ChaCha20 - cryptographic, and OS-free once seeded
// -------------------------------------------------------------

/* Set in `seeded` by seeding, and by nothing else. A zero-initialised generator - the shape of
 * "never seeded" - must not be usable, and `used` alone cannot say so: `used == 0` is exactly
 * what a zeroed struct holds, and the fill path read that as "a fresh block is ready", handing
 * the caller its own uninitialised block[]. */
#define CHACHA_MAGIC 0x5EEDu
/* The rest of `seeded` (RFC-0048): options, and the fork generation it was seeded in. */
#define CHACHA_PLAIN (1u << 16)          /* PROVEN_CHACHA_RNG_NO_ERASURE */
#define CHACHA_FORK (1u << 17)           /* seeded from entropy: a new key in a forked child */
#define CHACHA_MIB (1u << 18)            /* PROVEN_CHACHA_RNG_RESEED_EVERY_MIB */
#define CHACHA_GEN_SHIFT 20
#define CHACHA_GEN_MASK 0xFFFu           /* the child's count is the parent's plus one: 12 bits tell them apart */
#define CHACHA_EPOCH 16u                 /* blocks: the last of each is the next key */
#define CHACHA_MIB_BLOCKS 16384u         /* 1 MiB of keystream */

#define CHACHA_QR(a, b, c, d)                        \
    do {                                             \
        a += b; d ^= a; d = rotl32(d, 16);           \
        c += d; b ^= c; b = rotl32(b, 12);           \
        a += b; d ^= a; d = rotl32(d, 8);            \
        c += d; b ^= c; b = rotl32(b, 7);            \
    } while (0)

/* The keystream block of `state` as it stands (word 12 not stepped) into `out`, 64 bytes. */
static void chacha_block_to(const proven_u32 state[16], proven_byte_t *out) {
#ifndef PROVEN_FREESTANDING
    if (proven_sys_chacha_keystream_width() > 0) { proven_sys_chacha_keystream1(state, out); return; }
#endif
    proven_u32 x[16];
    for (int i = 0; i < 16; ++i) x[i] = state[i];

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
        proven_u32 v = x[i] + state[i];
        out[4 * i + 0] = (proven_byte_t)(v      );
        out[4 * i + 1] = (proven_byte_t)(v >>  8);
        out[4 * i + 2] = (proven_byte_t)(v >> 16);
        out[4 * i + 3] = (proven_byte_t)(v >> 24);
    }
}

/* Step the 32-bit block counter by `n` blocks. 2^32 blocks is 256 GiB of keystream from one
 * seed; past that the counter wraps and the stream would repeat, so carry into the first nonce
 * word rather than silently reusing keystream. */
static void chacha_step(proven_chacha_rng_t *g, proven_u32 n) {
    const proven_u32 before = g->state[12];
    g->state[12] = before + n;
    if (g->state[12] < before) ++g->state[13];
}

static bool chacha_ok(const proven_chacha_rng_t *g) { return (g->seeded & 0xFFFFu) == CHACHA_MAGIC; }

/* 32 bytes, little-endian, as the key words 4..11. */
static void chacha_set_key(proven_chacha_rng_t *g, const proven_byte_t k[32]) {
    for (int i = 0; i < 8; ++i) {
        g->state[4 + i] = (proven_u32)k[4 * i] | ((proven_u32)k[4 * i + 1] << 8) | ((proven_u32)k[4 * i + 2] << 16) | ((proven_u32)k[4 * i + 3] << 24);
    }
}

/* RFC-0048 E-1. `kb` holds the keystream of the epoch's last block (word 12 = 15 mod 16): its
 * first 32 bytes are the next key, the counter moves past it, and it is wiped. Every MiB, with
 * the option, 32 bytes from the entropy source are mixed in (XOR: as good as the better of the
 * two); a source that fails is skipped. */
/* With CHACHA_MIB, at each MiB of keystream - the key block just stepped past - new entropy into the key. */
static void chacha_mib_mix(proven_chacha_rng_t *g) {
    if ((g->seeded & CHACHA_MIB) && g->state[12] % CHACHA_MIB_BLOCKS == 0) {
        proven_byte_t e[32];
        if (proven_random_bytes(e, sizeof e)) {
            for (int i = 0; i < 8; ++i) {
                g->state[4 + i] ^= (proven_u32)e[4 * i] | ((proven_u32)e[4 * i + 1] << 8) | ((proven_u32)e[4 * i + 2] << 16) | ((proven_u32)e[4 * i + 3] << 24);
            }
        }
        secure_zero(e, sizeof e);
    }
}

static void chacha_rekey_from(proven_chacha_rng_t *g, proven_byte_t kb[64]) {
    chacha_set_key(g, kb);
    chacha_step(g, 1);
    chacha_mib_mix(g);
    secure_zero(kb, 64);
}

static void chacha_rekey(proven_chacha_rng_t *g) {
    proven_byte_t kb[64];
    chacha_block_to(g->state, kb);
    chacha_rekey_from(g, kb);
}

/* After a block was made: with erasure, the epoch's last data block is followed at once by the
 * new key - not at the next draw - so the old key is gone while its output is still in use. */
static void chacha_after_block(proven_chacha_rng_t *g) {
    if (!(g->seeded & CHACHA_PLAIN) && (g->state[12] & (CHACHA_EPOCH - 1)) == CHACHA_EPOCH - 1) chacha_rekey(g);
}

/* One 64-byte data block into the generator's buffer, then step the counter. */
static void chacha_block(proven_chacha_rng_t *g) {
    chacha_block_to(g->state, g->block);
    g->used = 0;
    chacha_step(g, 1);
    chacha_after_block(g);
}

/* RFC-0048 E-2: a generator seeded from entropy, in a process the fork generation says is not
 * the one it was seeded in, takes a new key before it draws - from the entropy source, or, if
 * that fails (a draw cannot), from its key and this process's id. What was buffered is dropped. */
#ifndef PROVEN_FREESTANDING
static void chacha_fork_rekey(proven_chacha_rng_t *g, proven_u32 gen);
#endif
static inline void chacha_fork_check(proven_chacha_rng_t *g) {
#ifndef PROVEN_FREESTANDING
    if (!(g->seeded & CHACHA_FORK)) return;
    const proven_u32 gen = (proven_u32)atomic_load_explicit(&proven_sys_fork_generation_, memory_order_relaxed) & CHACHA_GEN_MASK;
    if (gen != g->seeded >> CHACHA_GEN_SHIFT) chacha_fork_rekey(g, gen);
#else
    (void)g;
#endif
}

#ifndef PROVEN_FREESTANDING
static void chacha_fork_rekey(proven_chacha_rng_t *g, proven_u32 gen) {
    proven_byte_t k[64];
    if (!proven_random_bytes(k, 32)) {
        proven_u32 st[16];
        for (int i = 0; i < 16; ++i) st[i] = g->state[i];
        st[12] = 0; st[13] = 0; st[14] = 0xFFFFFFFFu; st[15] = (proven_u32)proven_sys_process_id();
        chacha_block_to(st, k);
        secure_zero(st, sizeof st);
    }
    chacha_set_key(g, k);
    secure_zero(k, sizeof k);
    g->state[12] = 0; g->state[13] = 0; g->state[14] = 0; g->state[15] = 0;
    secure_zero(g->block, sizeof g->block);
    g->used = 64;
    g->seeded = (g->seeded & ((1u << CHACHA_GEN_SHIFT) - 1u)) | (gen << CHACHA_GEN_SHIFT);
}
#endif

void proven_chacha_rng_seed_ex(proven_chacha_rng_t *g, const proven_byte_t seed[PROVEN_CHACHA_SEED_SIZE], unsigned options) {
    if (!g || !seed) return;

    /* "expand 32-byte k", as little-endian words. */
    g->state[0] = 0x61707865u;
    g->state[1] = 0x3320646Eu;
    g->state[2] = 0x79622D32u;
    g->state[3] = 0x6B206574u;
    chacha_set_key(g, seed);

    /* Counter 0, nonce 0. Every seed is a fresh key, so a fresh nonce buys nothing here - the
     * (key, nonce, counter) triple is unique because the key is (and, with erasure, every
     * epoch's key is new). */
    g->state[12] = 0;
    g->state[13] = 0;
    g->state[14] = 0;
    g->state[15] = 0;

    g->used = 64;   /* nothing buffered: the next draw generates a block */
    g->seeded = CHACHA_MAGIC | ((options & PROVEN_CHACHA_RNG_NO_ERASURE) ? CHACHA_PLAIN : 0u);
}

void proven_chacha_rng_seed(proven_chacha_rng_t *g, const proven_byte_t seed[PROVEN_CHACHA_SEED_SIZE]) {
    proven_chacha_rng_seed_ex(g, seed, 0u);
}

void proven_chacha_rng_fill(proven_chacha_rng_t *g, void *buf, proven_size_t len) {
    if (!g || !buf || len == 0) return;
    proven_byte_t *p = (proven_byte_t *)buf;

    /* Never seeded, or seeding failed. Hand back zeros - an obviously dead value - rather than
     * this frame's stack (which `used == 0` used to serve up as a "ready" block) or the fixed,
     * publicly derivable keystream of an all-zero key (which is what an advancing counter over
     * a zeroed state produces after the first block, and which passes every smell test). */
    if (!chacha_ok(g)) {
        for (proven_size_t i = 0; i < len; ++i) p[i] = 0;
        return;
    }
    chacha_fork_check(g);
    const bool plain = (g->seeded & CHACHA_PLAIN) != 0;

    /* What is left of the buffered block first; with erasure, what is handed out is wiped. */
    if (g->used < 64) {
        proven_size_t take = 64 - g->used;
        if (take > len) take = len;
#if (defined(__GNUC__) || defined(__clang__)) && !defined(PROVEN_FREESTANDING)
        __builtin_memcpy(p, g->block + g->used, take);
#else
        for (proven_size_t i = 0; i < take; ++i) p[i] = g->block[g->used + i];
#endif
        if (!plain) secure_zero(g->block + g->used, take);
        g->used += take;
        if (take == len) return;                    /* the common small draw: the block had it */
        p += take;
        len -= take;
    }

    /* A generator never rests on a key block (the key is taken as soon as the block before it
     * is made); one whose counter was set there by hand takes it now, as every path below would. */
    if (len > 0 && !plain && (g->state[12] & (CHACHA_EPOCH - 1)) == CHACHA_EPOCH - 1) chacha_rekey(g);

    /* Whole blocks straight into the caller's buffer: the same bytes, in the same order, as
     * through the buffer, without the copy. Several at once where the processor has vector
     * lanes. */
#ifndef PROVEN_FREESTANDING
    const int w = proven_sys_chacha_keystream_width();
    if (plain && len >= 256 && w > 0) {
        /* Never across a wrap of word 12 (which carries into word 13 between calls). */
        while (len >= 256) {
            proven_size_t n = (len / 64) & ~(proven_size_t)3;
            const proven_u32 to_wrap = 0u - g->state[12];   /* 0: 2^32 blocks before the wrap */
            if (to_wrap != 0 && n > to_wrap) n = to_wrap & ~(proven_u32)3;
            if (n == 0) break;                              /* within three blocks of it */
            if (n > ((proven_size_t)1 << 24)) n = (proven_size_t)1 << 24;
            proven_sys_chacha_keystream(g->state, p, n);
            chacha_step(g, (proven_u32)n);
            p += 64 * n;
            len -= 64 * n;
        }
    }
    /* With erasure, in runs of whole groups of four that stay inside an epoch (an epoch is
     * 16-aligned, so no run crosses the wrap of word 12): data blocks go straight out. The
     * epoch's last group - blocks 12 to 15, or 8 to 15 where eight lanes cost less than four -
     * holds the key block and is made aside: the data blocks still wanted are copied out, the
     * key taken, the rest wiped. */
    while (!plain && w > 0 && len >= 64) {
        const proven_u32 pos = g->state[12] & (CHACHA_EPOCH - 1);
        const proven_size_t left = (CHACHA_EPOCH - 1) - pos;           /* data blocks left in the epoch: 1..15 */
        const proven_u32 tail = w >= 8 ? CHACHA_EPOCH - 8 : CHACHA_EPOCH - 4;
        if (pos == 0 && w >= 8 && len >= 64 * CHACHA_EPOCH) {
            /* A whole epoch with room behind it: all 16 blocks straight into the buffer. The key
             * block lands where the next epoch's first block goes; it is read, and that block -
             * which there is room for, so it is written before this call returns - covers it. */
            proven_sys_chacha_keystream(g->state, p, CHACHA_EPOCH);
            chacha_step(g, CHACHA_EPOCH - 1);
            chacha_set_key(g, p + 64 * (CHACHA_EPOCH - 1));
            chacha_step(g, 1);
            /* As chacha_rekey_from, without a second step: stepping back and again would carry
             * into word 13 twice where word 12 has just wrapped. */
            chacha_mib_mix(g);
            p += 64 * (CHACHA_EPOCH - 1);
            len -= 64 * (CHACHA_EPOCH - 1);
        } else if (pos == 0 && w >= 8 && len >= 64 * (CHACHA_EPOCH - 1)) {
            proven_byte_t t[512];
            proven_sys_chacha_keystream(g->state, p, 8);
            chacha_step(g, 8);
            proven_sys_chacha_keystream(g->state, t, 8);
            for (int i = 0; i < 448; ++i) p[512 + i] = t[i];
            chacha_step(g, 7);
            chacha_rekey_from(g, t + 448);
            secure_zero(t, 448);
            p += 64 * (CHACHA_EPOCH - 1);
            len -= 64 * (CHACHA_EPOCH - 1);
        } else if (pos >= tail && len >= 64 * left) {
            /* The last group aside; `left` of its blocks (from `pos`) are still to go out. */
            proven_byte_t t[512];
            const proven_u32 n = CHACHA_EPOCH - tail;
            g->state[12] -= pos - tail;                                 /* back to the group's start: same epoch, no wrap */
            proven_sys_chacha_keystream(g->state, t, n);
            const proven_size_t skip = 64 * (pos - tail);
            for (proven_size_t i = 0; i < 64 * left; ++i) p[i] = t[skip + i];
            chacha_step(g, n - 1);
            chacha_rekey_from(g, t + 64 * (n - 1));
            secure_zero(t, 64 * (n - 1));
            p += 64 * left;
            len -= 64 * left;
        } else if (left >= 4 && len >= 256) {
            proven_size_t n = left & ~(proven_size_t)3;                     /* whole groups of data blocks */
            if (w >= 8 && n >= 8 && pos < tail) n = (tail - pos) & ~(proven_size_t)3;   /* up to the eight-lane tail */
            if (n == 0) n = 4;
            const proven_size_t fit = (len / 64) & ~(proven_size_t)3;
            if (n > fit) n = fit;
            proven_sys_chacha_keystream(g->state, p, n);
            chacha_step(g, (proven_u32)n);
            chacha_after_block(g);                                          /* ended on the key block: take it */
            p += 64 * n;
            len -= 64 * n;
        } else {
            break;
        }
    }
#endif
    while (len >= 64) {
        chacha_block_to(g->state, p);
        chacha_step(g, 1);
        chacha_after_block(g);
        p += 64;
        len -= 64;
    }

    /* The tail through the buffer, whose rest the next draw takes. */
    if (len > 0) {
        chacha_block(g);
        for (proven_size_t i = 0; i < len; ++i) p[i] = g->block[i];
        if (!plain) secure_zero(g->block, len);
        g->used = len;
    }
}

proven_u64 proven_chacha_rng_next(proven_chacha_rng_t *g) {
    if (!g || !chacha_ok(g)) return 0;
    chacha_fork_check(g);

    proven_u64 v = 0;
    if (g->used <= 56) {            /* the common case: eight bytes still in the block */
        proven_byte_t *q = g->block + g->used;
#if (defined(__GNUC__) || defined(__clang__)) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && !defined(PROVEN_FREESTANDING)
        __builtin_memcpy(&v, q, 8);                     /* the same little-endian word, in one load */
        if (!(g->seeded & CHACHA_PLAIN)) { __builtin_memset(q, 0, 8); __asm__ __volatile__("" : : "r"(q) : "memory"); }
#else
        for (int i = 0; i < 8; ++i) v |= (proven_u64)q[i] << (8 * i);
        if (!(g->seeded & CHACHA_PLAIN)) secure_zero(q, 8);
#endif
        g->used += 8;
    } else {
        proven_byte_t b[8];
        proven_chacha_rng_fill(g, b, sizeof b);
        for (int i = 0; i < 8; ++i) v |= (proven_u64)b[i] << (8 * i);
        secure_zero(b, sizeof b);
    }
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
    if (!g || !chacha_ok(g)) return (proven_rng_t){0};
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
 * The hook is meant to be installed once at startup, before any thread asks for a key, and no
 * lock here would make "swap the source while another thread is deriving a key" meaningful.
 * But it used to be two plain globals, so a program that did install it late had a data race
 * - undefined behaviour - and a reader could pair one call's function with the other call's
 * context (RFC-0009 S-004). Now the pair is published under a sequence count: a reader always
 * gets a function and the context installed with it, and there is no race to speak of. The
 * cost on the read side is three atomic loads.
 */
static _Atomic(proven_entropy_fn) g_entropy_fn = NULL;
static _Atomic(void *) g_entropy_ctx = NULL;
static atomic_uint g_entropy_seq = 0;   /* odd while an install is in progress */

void proven_random_set_source(proven_entropy_fn fn, void *ctx) {
    unsigned s = atomic_load_explicit(&g_entropy_seq, memory_order_relaxed);
    for (;;) {
        if ((s & 1u) == 0u &&
            atomic_compare_exchange_weak_explicit(&g_entropy_seq, &s, s + 1u,
                                                  memory_order_acquire, memory_order_relaxed)) {
            break;
        }
        s = atomic_load_explicit(&g_entropy_seq, memory_order_relaxed);
    }
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&g_entropy_fn, fn, memory_order_relaxed);
    atomic_store_explicit(&g_entropy_ctx, ctx, memory_order_relaxed);
    atomic_store_explicit(&g_entropy_seq, s + 2u, memory_order_release);
}

static proven_entropy_fn entropy_source(void **ctx) {
    for (;;) {
        unsigned s1 = atomic_load_explicit(&g_entropy_seq, memory_order_acquire);
        if (s1 & 1u) continue;
        proven_entropy_fn fn = atomic_load_explicit(&g_entropy_fn, memory_order_relaxed);
        void *c = atomic_load_explicit(&g_entropy_ctx, memory_order_relaxed);
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&g_entropy_seq, memory_order_relaxed) == s1) {
            *ctx = c;
            return fn;
        }
    }
}

bool proven_random_bytes(void *buf, proven_size_t len) {
    if (len == 0) return true;
    if (!buf) return false;

    void *ctx = NULL;
    proven_entropy_fn fn = entropy_source(&ctx);
    if (fn) return fn(ctx, buf, len);

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

bool proven_random_u64_checked(proven_u64 *out) {
    if (!out) return false;
    proven_u64 v = 0;
    if (!proven_random_bytes(&v, sizeof v)) {
        *out = 0;
        return false;
    }
    *out = v;
    return true;
}

bool proven_chacha_rng_seed_from_entropy(proven_chacha_rng_t *g) {
    return proven_chacha_rng_seed_from_entropy_ex(g, 0u);
}

bool proven_chacha_rng_seed_from_entropy_ex(proven_chacha_rng_t *g, unsigned options) {
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

    proven_chacha_rng_seed_ex(g, seed, options);
#ifndef PROVEN_FREESTANDING
    if (!(options & PROVEN_CHACHA_RNG_NO_FORK_RESEED)) {
        g->seeded |= CHACHA_FORK | (((proven_u32)proven_sys_fork_watch() & CHACHA_GEN_MASK) << CHACHA_GEN_SHIFT);
    }
#endif
    if ((options & PROVEN_CHACHA_RNG_RESEED_EVERY_MIB) && !(options & PROVEN_CHACHA_RNG_NO_ERASURE)) g->seeded |= CHACHA_MIB;

    /* Do not leave the seed lying in this frame - and do it in a way the optimiser cannot
     * throw away, because a plain loop here IS thrown away (the seed is never read again). */
    secure_zero(seed, sizeof seed);
    return true;
}

/* E-4: a generator of each thread's own. Thread-local, so no lock; seeded at the thread's first
 * call; erasure and the fork check as for any generator seeded from entropy. */
#ifndef PROVEN_FREESTANDING
static _Thread_local proven_chacha_rng_t t_fast;

bool proven_random_fast_bytes(void *buf, proven_size_t len) {
    if (len == 0) return true;
    if (!buf) return false;
    if (!chacha_ok(&t_fast) && !proven_chacha_rng_seed_from_entropy(&t_fast)) return false;
    proven_chacha_rng_fill(&t_fast, buf, len);
    return true;
}
#else
bool proven_random_fast_bytes(void *buf, proven_size_t len) { return proven_random_bytes(buf, len); }
#endif

unsigned proven_random_fork_generation(void) {
#ifndef PROVEN_FREESTANDING
    return proven_sys_fork_watch();
#else
    return 0;
#endif
}
