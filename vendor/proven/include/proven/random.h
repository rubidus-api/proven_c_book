#ifndef PROVEN_RANDOM_H
#define PROVEN_RANDOM_H

/**
 * @file random.h
 * @brief Randomness, by use case - and the one question you have to answer first.
 *
 * There is no single "random". There are two jobs that look identical and are not, and
 * choosing the wrong one is the whole danger:
 *
 * | Your job | Use | Why |
 * |---|---|---|
 * | A key, a token, a nonce - anything an attacker must not guess. | `proven_random_bytes`, or a `proven_chacha_rng_t` seeded from it. | Only a cryptographic source is unguessable. |
 * | The same, on a target with no OS. | `proven_chacha_rng_t`, seeded from the board's real entropy. | ChaCha20 is pure arithmetic; it needs no OS. It is only as unguessable as its seed. |
 * | A simulation, a test, a game, a load generator, a sample. | `proven_xoshiro256ss_t`. | Fast, and REPRODUCIBLE: the same seed replays the same run, which is what makes a failing test debuggable. |
 * | A number in a range, a shuffle, a float in [0,1). | `proven_rng_below`, `proven_rng_shuffle`, `proven_rng_f64`, over any source. | `% n` is biased, and everyone writes it anyway. These are not. |
 *
 * The rule, stated once: **`proven_xoshiro256ss_t` is not secret-grade and must never be used
 * as one.** It is fast *because* it is predictable - a few of its outputs reveal its entire
 * state, and therefore every number it will ever produce. That is a feature for a
 * reproducible simulation and a catastrophe for a session token. The two are given names that
 * cannot be confused, so the choice is visible at the call site rather than buried in a seed.
 *
 * @note This module used to say, in this header, that it deliberately offered no
 *       pseudo-random generator at all - that a fast PRNG and a secure one are different
 *       tools and shipping one under a name suggesting the other is how insecure tokens get
 *       written. That reasoning was right and is why the split above is explicit; the
 *       conclusion (offer nothing) was wrong, because a caller who needs a reproducible
 *       sequence does not stop needing one - they write `rand()`, or a hand-rolled LCG, and
 *       get something worse than what the library declined to give them. The answer is to
 *       offer both and make the distinction impossible to miss, not to offer neither.
 *
 * ## The trait
 *
 * `proven_rng_t` is a source of random bytes, and it is **infallible**: once you hold one,
 * drawing from it cannot fail. That is not a simplification - it is where the failure went.
 * Asking an operating system for entropy CAN fail, so that failure is confined to exactly one
 * place: seeding. You check it once, at startup, and every draw downstream is total.
 *
 * ## One seed, one use
 *
 * A generator is deterministic: the same seed gives the same stream. That is safe - the
 * standard generators (NIST SP 800-90A's DRBGs, OpenBSD's arc4random, the Linux kernel's) are
 * the same - as long as a seed is secret and **used once**. Used twice, it gives the same
 * "random" bytes twice: a nonce repeated under one AES-GCM or ChaCha20-Poly1305 key breaks
 * both secrecy and integrity, two ECDSA signatures with one nonce give away the private key,
 * and a token issued twice is valid twice. The ways a seed gets used twice:
 *
 * - **A seed of your own** (`proven_chacha_rng_seed`) is reproducible by definition: a forked
 *   child replays it. Bytes from `proven_random_bytes` passed to it lose the fork protection
 *   that `proven_chacha_rng_seed_from_entropy` gives - seed from entropy with that call.
 * - **Copying a generator** (it is a plain struct) clones its stream. Nothing can detect it.
 * - **A virtual machine snapshot restored, or cloned, and a process checkpoint (CRIU)** copy the
 *   memory, the key with it; a generator in user space cannot tell. `proven_random_bytes` does
 *   not have this problem where the kernel reseeds on such an event (Linux's VM generation ID).
 * - **A stored seed** - written to a log "for debugging", a file, an image - makes every output
 *   reproducible by whoever reads it.
 *
 * So: for a key, a token or a nonce in a long-running process, `proven_random_bytes`; for bulk,
 * or a target with no OS, a ChaCha generator seeded by `proven_chacha_rng_seed_from_entropy`;
 * a seed of your own for tests and replays only.
 *
 * ## Freestanding, and where entropy comes from
 *
 * Everything here is pure arithmetic and works on a bare-metal target - the generators, the
 * helpers, and `proven_random_bytes` itself. What differs is the **entropy source** behind it,
 * because that is the one thing a program cannot compute for itself:
 *
 * - **Hosted:** the OS CSPRNG is installed for you. `getrandom` on Linux, `getentropy` on the
 *   BSDs and macOS, `BCryptGenRandom` on Windows, `/dev/urandom` where none of those exist.
 *   You call nothing.
 * - **Bare metal:** there is no source until you install one, with
 *   `proven_random_set_source`. A board *has* real entropy - an on-chip TRNG, a ring
 *   oscillator, an ADC's noise floor - and the library cannot know where. Hand it over once at
 *   startup and `proven_chacha_rng_seed_from_entropy` turns those few hundred bytes into an
 *   endless cryptographic stream.
 *
 * With no source installed, `proven_random_bytes` returns **false**. It does not fall back to a
 * clock-seeded PRNG, because that looks like success and is a security hole nothing reports.
 * A refusal is a fact a caller can act on.
 */

#include "types.h"

// -------------------------------------------------------------
// The trait: an infallible source of random bytes
// -------------------------------------------------------------

typedef struct proven_rng_vtable_t {
    proven_u64 (*next_u64)(void *ctx);
    void (*fill)(void *ctx, void *buf, proven_size_t len);
} proven_rng_vtable_t;

/**
 * @brief A source of random bytes. Drawing from a valid one cannot fail.
 *
 * Two pointers; hold it by value. The context it points at is yours, and must outlive it.
 *
 * @warning If you implement this vtable yourself, `next_u64` must actually be a generator --
 *          its outputs must be spread over the whole 64-bit range. The helpers rely on that,
 *          and `proven_rng_below` relies on it in a way worth spelling out: it draws, and
 *          rejects the small slice of draws that would bias the result. A real generator lands
 *          outside that slice essentially always, so the loop runs once. A degenerate source --
 *          one that returns a constant, or only ever lands inside the rejection slice - makes
 *          that loop **spin forever**. No real generator can do this, and the two shipped here
 *          cannot; a broken hand-written one can. That is a caller bug, and it hangs loudly
 *          rather than quietly returning a biased number, which is the trade this makes on
 *          purpose: a hang you can find beats a bias you cannot.
 */
typedef struct {
    const proven_rng_vtable_t *vt;
    void *ctx;
} proven_rng_t;

[[nodiscard]]
static inline bool proven_rng_is_valid(proven_rng_t rng) {
    return rng.vt != NULL && rng.vt->next_u64 != NULL && rng.vt->fill != NULL && rng.ctx != NULL;
}

/** @brief One uniformly random 64-bit word. Returns 0 for an invalid source. */
[[nodiscard]]
proven_u64 proven_rng_u64(proven_rng_t rng);

/**
 * @brief Fill `len` bytes of `buf`. A no-op for `len == 0`, a NULL `buf`, or an invalid source.
 * @note Every byte is written; there is no short fill.
 */
void proven_rng_fill(proven_rng_t rng, void *buf, proven_size_t len);

// -------------------------------------------------------------
// Fast and reproducible: xoshiro256** - NOT for secrets
// -------------------------------------------------------------

/**
 * @brief A fast, reproducible generator. Never use it for anything that must not be guessed.
 *
 * xoshiro256** (Blackman & Vigna, public domain): 256 bits of state, period 2^256-1, and
 * output that passes the usual statistical batteries - which says nothing about whether an
 * adversary can predict it. They can; the state is recoverable from the output.
 */
typedef struct {
    proven_u64 s[4];
} proven_xoshiro256ss_t;

/**
 * @brief Seed from one 64-bit value. The same seed always replays the same sequence.
 *
 * @note The seed is expanded through SplitMix64, so a seed of 0 - or 1, 2, 3, which is what
 *       callers actually pass - still produces a well-distributed 256-bit state. Writing a
 *       small counter straight into the state words is the classic way to get a generator
 *       whose first outputs are visibly not random, and an all-zero state it can never leave.
 */
void proven_xoshiro256ss_seed(proven_xoshiro256ss_t *g, proven_u64 seed);

/** @brief The next word. The hot path: call this directly rather than through the trait. */
proven_u64 proven_xoshiro256ss_next(proven_xoshiro256ss_t *g);

/** @brief View this generator as a `proven_rng_t`, for the helpers below. */
[[nodiscard]]
proven_rng_t proven_xoshiro256ss_rng(proven_xoshiro256ss_t *g);

// -------------------------------------------------------------
// Cryptographic, and OS-free once seeded: ChaCha20
// -------------------------------------------------------------

/** @brief The seed a ChaCha generator takes: 256 bits, and they must be real entropy. */
#define PROVEN_CHACHA_SEED_SIZE 32

/**
 * @brief A cryptographic generator that runs with no operating system.
 *
 * ChaCha20 (Bernstein; RFC 8439) run as a stream - the keystream *is* the random data. Its
 * output is unguessable without the key, so unlike xoshiro it is safe for keys and tokens;
 * and it needs nothing from the OS once seeded, which is what makes it the answer on a
 * bare-metal target and the fast answer for bulk random data on a hosted one (no syscall per
 * draw).
 *
 * @note It is exactly as unguessable as its seed. Seed it from `proven_chacha_rng_seed_from_entropy`
 *       on a hosted target, or from a hardware entropy source on a board. Seeding it from the
 *       clock, a serial number, or an uninitialised buffer yields something that looks random
 *       and is not - which is worse than an obvious failure, because nothing reports it.
 *
 * **Key erasure (from v0.116.0, on unless PROVEN_CHACHA_RNG_NO_ERASURE).** The stream is cut
 * into epochs of 16 blocks (1 KiB of keystream): the last block of each is not handed out - its
 * first 32 bytes become the next epoch's key, and it is wiped - and bytes already handed out
 * are wiped from `block`. So someone who later reads the generator's memory can recompute at
 * most what the current epoch gave out (under 1 KiB), not anything before it. The stream is
 * still a function of the seed alone, whatever the sizes it is drawn in; its first 15 blocks
 * are RFC 8439's keystream for the seed as key, counter 0, nonce 0, and from the 16th it
 * differs from the plain keystream - which PROVEN_CHACHA_RNG_NO_ERASURE gives, as every release
 * before v0.116.0 did. Erasure does not hide the future: whoever holds the state predicts what
 * follows until a reseed.
 *
 * **Fork (from v0.116.0).** A generator seeded by proven_chacha_rng_seed_from_entropy notices,
 * at its first draw in the child of a fork, that it is in a new process, and takes a new key
 * from the entropy source there before drawing - so parent and child never hand out the same
 * bytes (unless PROVEN_CHACHA_RNG_NO_FORK_RESEED). If the source fails in the child, the new key
 * is derived from the old one and the child's process id: different from the parent's stream,
 * but not secret from the parent. One seeded with proven_chacha_rng_seed is reproducible by
 * definition, and a child replays it. POSIX only, through `pthread_atfork`: a raw `clone` or
 * `vfork` that bypasses the C library is not seen; Windows has no fork; a freestanding build
 * has none.
 */
typedef struct {
    proven_u32 state[16];
    proven_byte_t block[64];
    proven_size_t used;   /* bytes of `block` already handed out; 64 == "generate the next one" */

    /* Set by seeding, and by nothing else. A zero-initialised struct - which is the shape of
     * "never seeded" - must not be able to masquerade as a generator holding a fresh block, so
     * "is this usable" cannot be inferred from `used` alone. An unseeded generator, or one
     * whose seeding failed, is INERT: its trait is invalid, its next() is 0, and its fill()
     * writes zeros rather than whatever the stack was holding. */
    proven_u32 seeded;
} proven_chacha_rng_t;

/**
 * @brief The fork generation: a count that goes up by one in the child of every fork (POSIX,
 *        through `pthread_atfork`, registered at the first call); always 0 on Windows and in a
 *        freestanding build. For a generator kept outside this file that must notice, at a draw,
 *        that it is in a forked child - proven_crypto's AES generator (aes.h) - as
 *        proven_chacha_rng_t does: record it when seeding, compare it at every draw.
 */
[[nodiscard]]
unsigned proven_random_fork_generation(void);

/** @brief Options for proven_chacha_rng_seed_ex and proven_chacha_rng_seed_from_entropy_ex,
 *         OR-ed together. 0 is the default: key erasure on; for a generator seeded from entropy,
 *         a new key in a forked child; no periodic reseed. */
enum {
    /** The plain ChaCha20 keystream, as before v0.116.0: no key erasure, nothing wiped. */
    PROVEN_CHACHA_RNG_NO_ERASURE = 1u,
    /** Seeded from entropy: do not take a new key in a forked child (it replays the parent). */
    PROVEN_CHACHA_RNG_NO_FORK_RESEED = 2u,
    /** Seeded from entropy: mix 32 new bytes from the entropy source into the key every 1 MiB
     *  of stream, as OpenBSD's arc4random does, so that whoever read the state stops predicting
     *  the stream at the next MiB. A source that fails then is skipped (erasure still applies).
     *  It calls the source from inside a draw. Needs key erasure; ignored with NO_ERASURE. */
    PROVEN_CHACHA_RNG_RESEED_EVERY_MIB = 4u,
};

/** @brief Seed from 32 bytes of real entropy. The same seed replays the same stream. Options 0:
 *         key erasure on. */
void proven_chacha_rng_seed(proven_chacha_rng_t *g, const proven_byte_t seed[PROVEN_CHACHA_SEED_SIZE]);

/** @brief proven_chacha_rng_seed with options. The two that concern a generator seeded from
 *         entropy (NO_FORK_RESEED, RESEED_EVERY_MIB) mean nothing here and are ignored: this
 *         generator is reproducible. */
void proven_chacha_rng_seed_ex(proven_chacha_rng_t *g, const proven_byte_t seed[PROVEN_CHACHA_SEED_SIZE], unsigned options);

/** @brief The next word of keystream. */
proven_u64 proven_chacha_rng_next(proven_chacha_rng_t *g);

/**
 * @brief Fill `len` bytes. Faster than `next` in a loop: whole 64-byte blocks are written straight
 *        into `buf` - on x86 with GCC or Clang, eight (AVX2) or four (SSE2) at once, detected at
 *        run time; on AArch64, four at once with NEON. The bytes do not depend on how a stream is drawn: in one fill, in pieces of
 *        any size, or as words.
 */
void proven_chacha_rng_fill(proven_chacha_rng_t *g, void *buf, proven_size_t len);

/** @brief View this generator as a `proven_rng_t`, for the helpers below. */
[[nodiscard]]
proven_rng_t proven_chacha_rng(proven_chacha_rng_t *g);

// -------------------------------------------------------------
// Uniform helpers, over any source
// -------------------------------------------------------------

/**
 * @brief A uniformly random value in [0, bound). Returns 0 when `bound` is 0.
 *
 * `proven_rng_u64(rng) % bound` is what everyone writes, and it is BIASED unless `bound`
 * divides 2^64: the low values come up more often, by a margin invisible in a spot check and
 * real in a shuffle or a sample. This is Lemire's multiply-and-reject method - unbiased, and
 * in the overwhelmingly common case it rejects nothing at all.
 */
[[nodiscard]]
proven_u64 proven_rng_below(proven_rng_t rng, proven_u64 bound);

/**
 * @brief A uniformly random value in [lo, hi], inclusive at both ends.
 * @note Returns `lo` when `hi < lo`. The full span is representable: `lo == INT64_MIN` with
 *       `hi == INT64_MAX` does not overflow (the span is 2^64-1, computed in unsigned).
 */
[[nodiscard]]
proven_i64 proven_rng_range(proven_rng_t rng, proven_i64 lo, proven_i64 hi);

/**
 * @brief A uniformly random double in [0, 1).
 * @note 53 bits - the most a double's mantissa holds - so every representable value in the
 *       range is reachable and equally likely. It never returns 1.0.
 */
[[nodiscard]]
double proven_rng_f64(proven_rng_t rng);

/**
 * @brief Shuffle `count` elements of `elem_size` bytes into a uniformly random permutation.
 *
 * Fisher-Yates, drawing each index with `proven_rng_below`, so the permutation is unbiased --
 * unlike the `% n` shuffle, which measurably favours some orderings. Allocates nothing; swaps
 * in place. A `count` of 0 or 1 is a no-op.
 */
void proven_rng_shuffle(proven_rng_t rng, void *base, proven_size_t count, proven_size_t elem_size);

// -------------------------------------------------------------
// The entropy source - where a seed comes from
// -------------------------------------------------------------

/**
 * @brief A source of ENTROPY: the thing you seed a generator from.
 *
 * This is not a generator. A generator turns a seed into an endless stream; an entropy source
 * is the small amount of genuine unpredictability that the seed is made of, and it comes from
 * outside the program - a hardware noise circuit, an OS pool fed by interrupt timing. It is
 * the one thing a program cannot compute for itself, which is why it is the one thing that can
 * fail.
 *
 * @return false if it could not produce `len` bytes. `buf` must then not be used.
 */
typedef bool (*proven_entropy_fn)(void *ctx, void *buf, proven_size_t len);

/**
 * @brief Install the system entropy source.
 *
 * **On a hosted target one is already installed**: the OS CSPRNG (`getrandom` on Linux,
 * `getentropy` on the BSDs and macOS, `BCryptGenRandom` on Windows, `/dev/urandom` where none
 * of those exist). You do not have to call this, and you should not unless you have a reason.
 *
 * **On a bare-metal target there is none**, and this is how you supply one. A board has real
 * entropy - an on-chip TRNG, a ring-oscillator, an ADC's noise floor - and the library cannot
 * know where. Install it once at startup and everything above works: `proven_random_bytes`,
 * and `proven_chacha_rng_seed_from_entropy`, which is what turns a board's few hundred bytes
 * of hardware entropy into an endless cryptographic stream.
 *
 * @param fn  the source, or NULL to go back to the platform default (none, freestanding).
 * @param ctx passed to `fn` unchanged; may be NULL.
 *
 * @warning Install exactly one, once, before any thread draws from it. Installing later is not
 *          a data race - a draw always sees a function together with the context installed
 *          with it - but a program that swaps its entropy source while another thread is
 *          drawing a key cannot say which source that key came from.
 * @warning **Do not install a clock, a serial number, an uninitialised buffer, or a PRNG.**
 *          Those produce something that looks random and is not, which is worse than an
 *          obvious failure because nothing reports it. If the board has no real entropy, say
 *          so by installing nothing: a refusal is a fact a caller can act on.
 *
 * @note There is deliberately no built-in `RDRAND` / `RNDR` backend. On a hosted target the OS
 *       already mixes the CPU's instruction into its pool, so calling it directly buys nothing
 *       and costs you the OS's mixing; and a raw hardware instruction used as the sole source
 *       is exactly the arrangement people have argued about for a decade. If you want it, it
 *       is four lines behind this hook, and the choice is then visibly yours.
 */
void proven_random_set_source(proven_entropy_fn fn, void *ctx);

/**
 * @brief Fill `buf` with `len` cryptographically strong random bytes from the entropy source.
 *
 * On a hosted target this is the OS CSPRNG by default. Not a PRNG seeded from the clock, not
 * `rand()`; the real thing, suitable for keys, tokens, nonces, and for seeding the generators
 * above.
 *
 * @return true if `buf` was filled; false if there is no entropy source (a bare-metal target
 *         where none was installed) or the source failed. On false, `buf`'s contents are
 *         unspecified and MUST NOT be used - a caller that ignores the return and uses the
 *         buffer anyway is exactly the bug this boolean exists to prevent.
 *
 * @note len == 0 is a successful no-op (returns true).
 * @note This can block briefly, once, very early in a system's life, if the OS entropy pool is
 *       not yet initialised. It never returns low-quality bytes to avoid blocking - it waits,
 *       because bytes that are not random are worse than bytes that are late.
 * @note Drawing a large buffer straight from the source costs a syscall's worth of work per
 *       call. When you want bulk random data rather than a secret to seed with, seed a
 *       `proven_chacha_rng_t` from this once and draw from that.
 */
[[nodiscard]]
bool proven_random_bytes(void *buf, proven_size_t len);

/**
 * @brief A single cryptographically strong 64-bit value, or 0 on failure.
 *
 * A convenience over proven_random_bytes for one word where a failed draw does no harm - a
 * hash seed that has a fallback, an identifier that only has to be unlikely to repeat.
 *
 * @warning Not for a secret. When the source fails - a bare-metal target with no source
 *          installed is the common case - this returns 0, and a token or key of 0 is the
 *          predictable secret the rest of this header exists to prevent. Nothing reports it.
 *          Use proven_random_u64_checked, which says when it failed.
 */
[[nodiscard]]
proven_u64 proven_random_u64(void);

/**
 * @brief A single cryptographically strong 64-bit value, and whether there is one.
 *
 * The form to use for a token, a key, a nonce: anything whose value must not be guessable.
 *
 * @return true and `*out` set from the entropy source; false if there is no source or it
 *         failed, with `*out` set to 0 - which must then not be used as a secret. false, and
 *         nothing written, when `out` is NULL.
 */
[[nodiscard]]
bool proven_random_u64_checked(proven_u64 *out);

/**
 * @brief Seed a ChaCha generator from the entropy source - the OS, or the one you installed.
 *
 * This is the call that makes the cryptographic generator usable on a board: a few hundred
 * bytes of real hardware entropy become an endless keystream that needs nothing further.
 *
 * @return false if there is no entropy source, or it failed. The generator is left INERT - it
 *         is not merely unseeded, it refuses to produce anything - so a caller who ignores this
 *         boolean gets zeros rather than plausible bytes. This is the one place randomness is
 *         allowed to fail; check it here, once, and every draw afterwards is infallible.
 */
[[nodiscard]]
bool proven_chacha_rng_seed_from_entropy(proven_chacha_rng_t *g);

/** @brief proven_chacha_rng_seed_from_entropy with options (PROVEN_CHACHA_RNG_*). */
[[nodiscard]]
bool proven_chacha_rng_seed_from_entropy_ex(proven_chacha_rng_t *g, unsigned options);

/**
 * @brief Fill `buf` with `len` cryptographically strong bytes from a generator of this thread's
 *        own, seeded from the entropy source the first time this thread calls it.
 *
 * The generator is a proven_chacha_rng_t seeded by proven_chacha_rng_seed_from_entropy with the
 * default options - key erasure, a new key in a forked child - so after the first call no
 * system call is made. Use it for bulk: on Linux with kernel 6.11 and glibc 2.41 or later,
 * proven_random_bytes runs in user space too and is as fast for a few bytes (measured on the
 * build host: 38 ns for 8 bytes, against 1.65 us for 1 KiB, where this takes about a quarter);
 * on older systems proven_random_bytes makes a system call each time. For a long-term secret
 * prefer proven_random_bytes: the operating system reseeds its generator; this one is reseeded
 * only in a forked child. Its state stays in the thread's memory, not wiped when the thread
 * ends.
 *
 * @return false when the entropy source failed at the first call (nothing is drawn then, and
 *         the next call tries again); true and `buf` filled otherwise. len == 0 is true.
 * @note In a freestanding build there are no threads to keep it for: this is proven_random_bytes.
 */
[[nodiscard]]
bool proven_random_fast_bytes(void *buf, proven_size_t len);

#endif /* PROVEN_RANDOM_H */
