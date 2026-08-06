#ifndef PROVEN_HASH_H
#define PROVEN_HASH_H

/**
 * @file hash.h
 * @brief Hashing, by use case.
 *
 * There is no single "hash" - the right function depends entirely on what you are doing
 * with the result, and picking the wrong one is how you get either a slow program or an
 * insecure one. This module offers exactly one primitive per job, so the choice is made
 * for you once you know the job:
 *
 * | You are... | Use | Why not the others |
 * |---|---|---|
 * | hashing keys into YOUR OWN hash table, trusted input | `proven_hash_bytes` (FNV-1a) | fast; a cryptographic hash would be 50x slower for no benefit |
 * | hashing keys from UNTRUSTED input into a table | `proven_hash_keyed` (SipHash-2-4) | FNV lets an attacker collide every key into one bucket and turn your O(1) table into O(n²) |
 * | checking data did not get CORRUPTED in transit or on disk | `proven_crc32` | a checksum, not a hash; interoperates with gzip/zlib/PNG, which all use this exact CRC |
 * | fingerprinting content: dedup, content-addressing, "are these two files the same" | `proven_sha256` | the only one here safe against a deliberately-constructed collision |
 *
 * The line that matters most: **CRC-32 and FNV are not security functions.** They detect
 * accident, not attack. Do not use them to decide whether two things are "the same" when
 * someone might benefit from fooling you - that is what `proven_sha256` is for.
 *
 * Every function here is byte-exact and platform-independent: the same input gives the same
 * output on a 32-bit target, a 64-bit target, big-endian or little-endian. A hash you
 * persist or send over a wire has to be, or it is not a fingerprint of the content - it is a
 * fingerprint of the machine.
 *
 * These are all published, royalty-free algorithms (FNV: public domain; SipHash: CC0;
 * CRC-32/IEEE: public domain; SHA-256: FIPS 180-4, unpatented), implemented here from their
 * specifications and checked against each one's official known-answer vectors.
 */

#include "types.h"
#include "memory.h"

// -----------------------------------------------------------------------------
// Non-cryptographic: for your own hash tables
// -----------------------------------------------------------------------------

/**
 * @brief FNV-1a, 64-bit. Fast, well-distributed, and NOT secure. For hashing keys into
 *        your own table when the input is trusted.
 *
 * This is the same family `map` uses internally. Exposed because a caller building its own
 * open-addressed table, or deduplicating a batch of trusted blobs, should not have to
 * reach for a cryptographic digest (slow) or hand-roll a mixer (a coin-flip on quality).
 *
 * @warning An attacker who chooses the input can make every value collide. If the bytes
 *          come from outside your program - request headers, file names, network data -
 *          use proven_hash_keyed instead. The cost of guessing wrong here is a table that
 *          degrades to a linked list under load, on purpose, at a time of the attacker's
 *          choosing.
 */
[[nodiscard]]
proven_u64 proven_hash_bytes(proven_mem_view_t data);

/**
 * @brief SipHash-2-4, keyed. A fast hash an attacker cannot predict without the key.
 *
 * The same job as proven_hash_bytes - hashing keys into a table - but for input you do not
 * control. With a per-process random `key`, an attacker cannot construct keys that all land
 * in one bucket, because they cannot compute the hash without the key. This is the function
 * Python, Rust, and the Linux kernel reach for to make their hash tables safe by default.
 *
 * @param key A 16-byte secret. Choose it once at startup from a real source of randomness
 *            and keep it for the process's lifetime; a fixed or guessable key defeats the
 *            entire point. The library does not pick it for you, because where your entropy
 *            comes from is your decision to make, not one to have made silently.
 */
[[nodiscard]]
proven_u64 proven_hash_keyed(proven_mem_view_t data, const proven_byte_t key[16]);

// -----------------------------------------------------------------------------
// Checksum: for detecting corruption
// -----------------------------------------------------------------------------

/**
 * @brief CRC-32 (IEEE 802.3, the one gzip/zlib/PNG use). A checksum for accidental damage.
 *
 * Answers "did these bytes get mangled in transit or on disk?" - a flipped bit, a truncated
 * transfer, a bad sector. It is good at that and interoperates with the enormous amount of
 * existing data that carries this exact CRC.
 *
 * @warning It is trivial to construct two different inputs with the same CRC-32. It detects
 *          accidents, never attacks. If the question is "did someone TAMPER with this",
 *          the answer is proven_sha256, not this.
 */
[[nodiscard]]
proven_u32 proven_crc32(proven_mem_view_t data);

/**
 * @brief Streaming CRC-32: fold more bytes into a running value.
 *
 * Start from 0 for the first chunk, feed the result back in for each subsequent chunk, and
 * the final value equals proven_crc32 over the concatenation. For checksumming a stream you
 * cannot hold in memory all at once.
 *
 * @param crc  The running value; 0 to begin.
 * @param data The next chunk.
 */
[[nodiscard]]
proven_u32 proven_crc32_update(proven_u32 crc, proven_mem_view_t data);

// -----------------------------------------------------------------------------
// Cryptographic digest: for fingerprinting content
// -----------------------------------------------------------------------------

/** @brief Bytes in a SHA-256 digest. */
#define PROVEN_SHA256_SIZE ((proven_size_t)32)

/**
 * @brief A SHA-256 hashing context. Opaque; use the init/update/final calls.
 *
 * Its fields are exposed only so it can live on the stack with no allocation. Do not read or
 * write them - the layout is not part of the contract and the running state is meaningless
 * between update calls.
 */
typedef struct {
    proven_u32  state[8];
    proven_u64  length;      /* total bytes fed, for the length padding */
    proven_byte_t block[64]; /* the partial 64-byte block not yet compressed */
    proven_size_t block_len;
} proven_sha256_t;

/** @brief Begin a SHA-256 digest. */
void proven_sha256_init(proven_sha256_t *ctx);

/**
 * @brief Feed more bytes into a digest in progress. Call as many times as you like; the
 *        result depends only on the concatenation of everything fed, not on how it was split.
 */
void proven_sha256_update(proven_sha256_t *ctx, proven_mem_view_t data);

/**
 * @brief Finish the digest, writing 32 bytes to `out`. The context is spent afterwards;
 *        re-init it to hash something else.
 */
void proven_sha256_final(proven_sha256_t *ctx, proven_byte_t out[PROVEN_SHA256_SIZE]);

/**
 * @brief One-shot SHA-256 of a single buffer. Equivalent to init + one update + final.
 *
 * The cryptographic fingerprint the whole module exists to provide: content-addressing, dedup,
 * "are these two files identical" answered safely against a party trying to make two
 * different files look identical. This is what a content-addressed build cache or IR store
 * hashes with.
 */
void proven_sha256(proven_mem_view_t data, proven_byte_t out[PROVEN_SHA256_SIZE]);

/**
 * @brief Write a digest as 64 lowercase hex characters plus a NUL, into `out` (>= 65 bytes).
 *
 * A digest is bytes; a fingerprint people paste into logs, filenames, and URLs is hex. This
 * is the spelling everyone else uses (git, sha256sum), so it interoperates.
 */
void proven_sha256_to_hex(const proven_byte_t digest[PROVEN_SHA256_SIZE], char out[65]);

#endif /* PROVEN_HASH_H */
