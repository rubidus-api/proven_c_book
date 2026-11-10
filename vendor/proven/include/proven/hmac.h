#ifndef PROVEN_HMAC_H
#define PROVEN_HMAC_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/hash.h"
#include "proven/hash_legacy.h"

/**
 * @file hmac.h
 * @brief HMAC (RFC 2104), HKDF (RFC 5869) and PBKDF2 (RFC 8018) over SHA-256, SHA-384 and
 *        SHA-512 - and SHA-1, for the formats that still name it (PKCS #12, old signatures).
 *
 * Two different jobs that are built from the same piece:
 *
 *   - **HMAC answers "did someone who knows the key produce this message?"** A digest alone
 *     cannot: anyone can hash anything. HMAC mixes a secret key into the digest so that only a
 *     holder of the key can compute it - a message authentication code. Use it to sign a
 *     cookie, a webhook, a token, a message between two parties that share a key.
 *   - **HKDF turns key material into keys.** What comes out of a key exchange, or a master
 *     secret, is not yet a key for anything: HKDF extracts a uniform key from it and expands
 *     that into as many separate keys as a protocol needs, each bound to a label so that a key
 *     for one purpose is useless for another.
 *
 * Neither is for passwords. A password has little entropy, and both of these are fast - an
 * attacker tries billions a second. Stretching a password needs a function made slow on
 * purpose: proven_pbkdf2 is the one here (since v0.109.0), for reading what other tools wrote
 * (PKCS #8 and PKCS #12 key files); scrypt and Argon2 are not in this library.
 *
 * Pure computation: no allocation, no OS; part of the freestanding profile.
 */

typedef enum {
    PROVEN_HMAC_SHA256 = 0,
    PROVEN_HMAC_SHA384,
    PROVEN_HMAC_SHA512,
    /** SHA-1 (v0.109.0): for PBKDF2 in PKCS #12 files, RSA signatures that name it, and old
     *  signatures. Its collisions are practical: never for a new signature or a new design. */
    PROVEN_HMAC_SHA1
} proven_hmac_hash_t;

/** @brief The largest MAC any of the three produces: a buffer of this size always fits. */
#define PROVEN_HMAC_MAX_SIZE ((proven_size_t)64)

/** @brief Bytes in a MAC made with `hash`: 32, 48, 64, or 20 for SHA-1. 0 for a value that is not one of them. */
[[nodiscard]]
proven_size_t proven_hmac_size(proven_hmac_hash_t hash);

/**
 * @brief A running HMAC. Caller-owned state; it allocates nothing. It holds key material, and
 *        proven_hmac_final wipes it.
 */
typedef struct {
    proven_hmac_hash_t hash;
    union {
        proven_sha256_t s256;
        proven_sha512_t s512;
        proven_sha1_t s1;
    } inner;
    proven_byte_t outer_key[128];   /* the key XORed with the outer pad, kept for the second pass */
    bool ready;
} proven_hmac_t;

/**
 * @brief Begin a MAC with `key`.
 *
 * A key of any length is accepted (one longer than the hash's block is hashed first, as the
 * RFC says), but a key shorter than the MAC is weaker than the MAC: use at least
 * proven_hmac_size(hash) bytes from a cryptographic source.
 *
 * @return PROVEN_ERR_INVALID_ARG for a hash that is not one of the four, or a null key with a
 *         non-zero size.
 */
[[nodiscard]]
proven_err_t proven_hmac_init(proven_hmac_t *hmac, proven_hmac_hash_t hash, proven_mem_view_t key);

/** @brief Feed more of the message. The result depends only on the bytes, not on how they were
 *         split. Ignored on a state that was not successfully begun. */
void proven_hmac_update(proven_hmac_t *hmac, proven_mem_view_t data);

/**
 * @brief Finish: write proven_hmac_size(hash) bytes to `out` and wipe the state.
 *
 * To check a MAC you received, compute your own and compare with proven_mem_equal_ct - never
 * with memcmp, which tells an attacker how many leading bytes of a guess were right.
 */
void proven_hmac_final(proven_hmac_t *hmac, proven_byte_t out[PROVEN_HMAC_MAX_SIZE]);

/** @brief One-shot HMAC of a single buffer. Errors as proven_hmac_init. */
[[nodiscard]]
proven_err_t proven_hmac(proven_hmac_hash_t hash, proven_mem_view_t key, proven_mem_view_t data,
                         proven_byte_t out[PROVEN_HMAC_MAX_SIZE]);

/**
 * @brief HKDF step one: condense input key material into a pseudorandom key.
 *
 * @param salt optional and not secret; an empty salt is a string of zeros, as the RFC defines.
 * @param ikm the input key material: a shared secret from a key exchange, a master key.
 * @param prk receives proven_hmac_size(hash) bytes.
 * @return PROVEN_ERR_INVALID_ARG for an unknown hash or a null pointer with a non-zero size.
 */
[[nodiscard]]
proven_err_t proven_hkdf_extract(proven_hmac_hash_t hash, proven_mem_view_t salt, proven_mem_view_t ikm,
                                 proven_byte_t prk[PROVEN_HMAC_MAX_SIZE]);

/**
 * @brief HKDF step two: expand a pseudorandom key into `out.size` bytes of key bound to `info`.
 *
 * Different `info` gives unrelated output from the same key: that is how one secret becomes
 * an encryption key, a MAC key and an IV without any of them revealing another.
 *
 * @param prk at least proven_hmac_size(hash) bytes - the output of proven_hkdf_extract, or a
 *        key that is already uniformly random.
 * @return PROVEN_ERR_OUT_OF_BOUNDS when more than 255 times the hash size is asked for (the
 *         construction's limit); PROVEN_ERR_INVALID_ARG for an unknown hash, a `prk` shorter
 *         than the hash, or a null pointer with a non-zero size. Nothing is written on error.
 */
[[nodiscard]]
proven_err_t proven_hkdf_expand(proven_hmac_hash_t hash, proven_mem_view_t prk, proven_mem_view_t info,
                                proven_mem_mut_t out);

/** @brief Both steps: proven_hkdf_extract, then proven_hkdf_expand. */
[[nodiscard]]
proven_err_t proven_hkdf(proven_hmac_hash_t hash, proven_mem_view_t salt, proven_mem_view_t ikm,
                         proven_mem_view_t info, proven_mem_mut_t out);

/**
 * @brief PBKDF2 (RFC 8018 section 5.2) with HMAC over `hash`: `out.size` bytes of key from a
 *        password and a salt, the HMAC iterated `iterations` times for each block.
 *
 * For reading key files that other tools encrypted (PBES2 in PKCS #8, PKCS #12 with
 * PBKDF2); for storing passwords prefer a memory-hard function. The password may be any bytes
 * (PKCS #12's BMPString form is the caller's to make). The work is iterations times the blocks:
 * a caller reading an untrusted file bounds `iterations` itself.
 *
 * @return PROVEN_ERR_INVALID_ARG for an unknown hash, `iterations` 0, an empty `out`, or a null
 *         pointer with a non-zero size; PROVEN_ERR_OUT_OF_BOUNDS when `out` is longer than
 *         (2^32 - 1) blocks. Nothing is written on error.
 */
[[nodiscard]]
proven_err_t proven_pbkdf2(proven_hmac_hash_t hash, proven_mem_view_t password, proven_mem_view_t salt,
                           proven_u32 iterations, proven_mem_mut_t out);

#endif /* PROVEN_HMAC_H */
