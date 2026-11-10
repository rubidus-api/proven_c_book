#ifndef PROVEN_SIGN_H
#define PROVEN_SIGN_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/allocator.h"
#include "proven/hmac.h"
#include "proven/random.h"

/**
 * @file sign.h
 * @brief Private keys, signatures over a digest, and their verification: RSA (PKCS #1 v1.5 and
 *        PSS, RFC 8017) and ECDSA over P-256 and P-384 (FIPS 186-5, nonces of RFC 6979). Since
 *        v0.109.0; the code is the one the TLS engine signs and verifies with.
 *
 * What is signed is a digest the caller computed (proven_sha256 and the rest), named by a
 * proven_hmac_hash_t. Signing is deterministic where the scheme is - RSA v1.5 and ECDSA with
 * RFC 6979 - so the same key, digest and scheme give the same bytes in every version. A PSS
 * signature takes a salt from the caller's generator.
 *
 * Secrets: a key's private parts are SECRET. Signing runs in time that does not depend on them
 * (an RSA signature is also blinded, and checked against the public key before it is given
 * out, so a fault in the computation cannot leak a prime). proven_pkey_destroy wipes the key.
 * Verification handles nothing secret.
 *
 * Freestanding: nothing here calls the OS. The randomness RSA blinding and PSS need comes from
 * the caller's proven_rng_t, which must be a cryptographic generator (proven_chacha_rng seeded
 * from the system - never xoshiro).
 */

/** @brief What a key is. */
typedef enum {
    PROVEN_PKEY_NONE = 0,
    PROVEN_PKEY_RSA,        /**< 2048 to 4096 bits, two primes. */
    PROVEN_PKEY_P256,       /**< ECDSA over NIST P-256 (secp256r1). */
    PROVEN_PKEY_P384        /**< ECDSA over NIST P-384 (secp384r1). */
} proven_pkey_kind_t;

/** @brief How a signature is made and encoded. */
typedef enum {
    PROVEN_SIGN_RSA_PKCS1 = 1,  /**< RSASSA-PKCS1-v1_5: the DigestInfo of the hash, then the digest. */
    PROVEN_SIGN_RSA_PSS,        /**< RSASSA-PSS, MGF1 with the same hash, a salt as long as the digest. */
    PROVEN_SIGN_ECDSA,          /**< ECDSA, DER: SEQUENCE { INTEGER r, INTEGER s } (X.509, CMS). */
    PROVEN_SIGN_ECDSA_RAW,      /**< ECDSA, r then s, each the curve's size (JOSE, raw). */
    PROVEN_SIGN_RSA_PKCS1_RAW   /**< v1.5 padding over the bytes given, no DigestInfo: TLS 1.0 and 1.1's MD5 | SHA-1 (36 bytes). For nothing else (v0.111.0). */
} proven_sign_scheme_t;

/** @brief Bytes that hold any signature made here: a 4096-bit RSA one. */
#define PROVEN_SIGN_MAX_SIZE ((proven_size_t)512)

/** @brief A private key. Made by proven_pkey_parse or proven_pkey_from_*. Signing may run on
 *         several threads at once (since v0.111.0; an RSA key's blinding pair moves under a
 *         flag held for one squaring); destroying it may not. */
typedef struct proven_pkey proven_pkey_t;

/** @brief The parts of an RSA private key, each an unsigned big-endian integer (leading zeros
 *         allowed). `d` may be empty: signing uses the CRT parts. */
typedef struct {
    proven_mem_view_t n, e, d, p, q, dp, dq, qinv;
} proven_rsa_components_t;

/**
 * @brief Read a private key: PEM ("PRIVATE KEY" - PKCS #8, unencrypted - "RSA PRIVATE KEY" or
 *        "EC PRIVATE KEY"; the first such block in `text`) or DER of any of the three - DER
 *        when `text` has no "-----BEGIN" line.
 *
 * An RSA key is checked for its shape (n = p q, the CRT values in range) and then signs once
 * and verifies that, so a key that cannot sign is refused here. `rng` makes RSA's blinding pair;
 * it is not used for an EC key and may then be zero.
 *
 * @return PROVEN_ERR_INVALID_FORMAT for something that is not such a key; PROVEN_ERR_UNSUPPORTED
 *         for a key of another algorithm or size (Ed25519, P-521, RSA under 2048 bits or over
 *         4096, an encrypted PKCS #8 - which proven_pbkdf2 and proven_aes_cbc_decrypt can open);
 *         PROVEN_ERR_INVALID_ARG; PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_pkey_parse(proven_allocator_t alloc, proven_mem_view_t text, proven_rng_t rng, proven_pkey_t **out);

/** @brief An RSA key from its parts. Checked and tried as proven_pkey_parse does. */
[[nodiscard]]
proven_err_t proven_pkey_from_rsa(proven_allocator_t alloc, const proven_rsa_components_t *parts, proven_rng_t rng, proven_pkey_t **out);

/** @brief An EC key from its private scalar (big-endian, the curve's size: 32 or 48 bytes).
 *         PROVEN_ERR_INVALID_FORMAT when it is zero or not below the group's order. */
[[nodiscard]]
proven_err_t proven_pkey_from_ec(proven_allocator_t alloc, proven_pkey_kind_t curve, proven_mem_view_t scalar, proven_pkey_t **out);

/** @brief Wipe and free. NULL is a no-op. */
void proven_pkey_destroy(proven_pkey_t *key);

/** @brief What the key is; PROVEN_PKEY_NONE for NULL. */
[[nodiscard]]
proven_pkey_kind_t proven_pkey_kind(const proven_pkey_t *key);

/** @brief The modulus's or the curve's size in bits: 2048..4096, 256, 384. */
[[nodiscard]]
proven_size_t proven_pkey_bits(const proven_pkey_t *key);

/** @brief The RSA parts, as views into the key (valid until it is destroyed). `d` is empty when
 *         the key was made without it. PROVEN_ERR_INVALID_STATE for an EC key. SECRET. */
[[nodiscard]]
proven_err_t proven_pkey_rsa_components(const proven_pkey_t *key, proven_rsa_components_t *out);

/** @brief The EC private scalar, a view into the key. PROVEN_ERR_INVALID_STATE for RSA. SECRET. */
[[nodiscard]]
proven_err_t proven_pkey_ec_scalar(const proven_pkey_t *key, proven_mem_view_t *out);

/** @brief The EC public point, uncompressed: 04 | X | Y (65 or 97 bytes), a view into the key. */
[[nodiscard]]
proven_err_t proven_pkey_ec_point(const proven_pkey_t *key, proven_mem_view_t *out);

/**
 * @brief The public key as a SubjectPublicKeyInfo, DER - what a certificate carries and what
 *        proven_verify_spki reads. RSA: rsaEncryption with a NULL parameter; EC: id-ecPublicKey
 *        with the named curve and the uncompressed point.
 * @return PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small (600 bytes always suffice), with
 *         `*len` 0.
 */
[[nodiscard]]
proven_err_t proven_pkey_spki(const proven_pkey_t *key, proven_mem_mut_t out, proven_size_t *len);

/**
 * @brief Sign a digest.
 *
 * @param hash the hash that made `digest`; `digest.size` must be its size. SHA-1 is accepted
 *        for RSA v1.5 and ECDSA, for formats that still ask for it. Ignored by
 *        PROVEN_SIGN_RSA_PKCS1_RAW, whose `digest` is the bytes to pad (at most the modulus's
 *        size less 11).
 * @param rng only PROVEN_SIGN_RSA_PSS uses it (the salt); otherwise it may be zero.
 * @param out at least the signature's size: RSA the modulus's bytes, ECDSA raw twice the
 *        curve's, ECDSA DER up to twice the curve's plus 9. PROVEN_SIGN_MAX_SIZE always does.
 * @return PROVEN_ERR_INVALID_ARG for a scheme the key cannot make (ECDSA with RSA, and the
 *         other way), a digest of the wrong size, or an invalid `rng` for PSS;
 *         PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small; PROVEN_ERR_INVALID_STATE when the
 *         signature did not verify against the key (a fault), and nothing is given out.
 */
[[nodiscard]]
proven_err_t proven_pkey_sign(proven_pkey_t *key, proven_sign_scheme_t scheme, proven_hmac_hash_t hash, proven_mem_view_t digest,
                              proven_rng_t rng, proven_mem_mut_t out, proven_size_t *len);

/**
 * @brief Verify an RSA signature (v1.5, or PSS with a salt of `salt_len` bytes) under (n, e).
 * @param salt_len PSS only: the salt's length, usually the digest's (RFC 4055 default 20 is
 *        SHA-1's); ignored for v1.5. PROVEN_SIGN_RSA_PKCS1_RAW ignores `hash` too and takes
 *        `digest` as the padded bytes themselves.
 * @return PROVEN_OK; PROVEN_ERR_UNTRUSTED when it does not verify; PROVEN_ERR_UNSUPPORTED for a
 *         modulus outside 2048..8192 bits; PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_verify_rsa(proven_mem_view_t n, proven_mem_view_t e, proven_sign_scheme_t scheme, proven_hmac_hash_t hash,
                               proven_size_t salt_len, proven_mem_view_t digest, proven_mem_view_t sig);

/**
 * @brief Verify an ECDSA signature (DER or raw, as `scheme` says) under the uncompressed point.
 *        The DER form is read strictly. The hash only says how long the digest is.
 * @return PROVEN_OK; PROVEN_ERR_UNTRUSTED; PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_verify_ecdsa(proven_pkey_kind_t curve, proven_mem_view_t point, proven_sign_scheme_t scheme,
                                 proven_mem_view_t digest, proven_mem_view_t sig);

/**
 * @brief Verify with the key in a SubjectPublicKeyInfo (a certificate's `public_key_info`): RSA
 *        or EC P-256/P-384, by what it says. PSS takes a salt as long as the digest.
 * @return as the two above; PROVEN_ERR_UNSUPPORTED for a key of another algorithm;
 *         PROVEN_ERR_INVALID_FORMAT for a SubjectPublicKeyInfo that does not parse.
 */
[[nodiscard]]
proven_err_t proven_verify_spki(proven_mem_view_t spki, proven_sign_scheme_t scheme, proven_hmac_hash_t hash,
                                proven_mem_view_t digest, proven_mem_view_t sig);

// -------------------------------------------------------------
// Keys held by the caller (since v0.111.0)
// -------------------------------------------------------------

/**
 * The functions below take the private value itself, for a caller that already keeps it - a
 * protocol engine with a key in its own configuration, or a key made for one exchange. The same
 * SECRET rules hold: wipe the value with proven_mem_wipe when done.
 */

/** @brief The Ed25519 public key (RFC 8032) of a 32-byte seed. */
void proven_ed25519_public(proven_byte_t pub[32], const proven_byte_t seed[32]);

/** @brief An Ed25519 signature of `msg` (the message itself, not a digest). `pub` is the seed's
 *         public key, as proven_ed25519_public made it. Deterministic. */
void proven_ed25519_sign(proven_byte_t sig[64], const proven_byte_t seed[32], const proven_byte_t pub[32], proven_mem_view_t msg);

/**
 * @brief Verify an Ed25519 signature. S not below the group's order, and a public key or R that
 *        is not the canonical encoding of a point, are refused.
 * @return PROVEN_OK; PROVEN_ERR_UNTRUSTED; PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_ed25519_verify(const proven_byte_t pub[32], proven_mem_view_t msg, const proven_byte_t sig[64]);

/**
 * @brief ECDSA with a private scalar (big-endian, the curve's size), r then s into `rs`
 *        (twice the curve's size). The nonce is RFC 6979's, made with HMAC over `nonce_hash`;
 *        `digest` may be of any length (TLS 1.0 signs a SHA-1 digest with SHA-256's nonce).
 * @return PROVEN_ERR_INVALID_FORMAT for a scalar of zero or not below the order;
 *         PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_ecdsa_sign_scalar(proven_pkey_kind_t curve, proven_hmac_hash_t nonce_hash, proven_mem_view_t scalar,
                                      proven_mem_view_t digest, proven_byte_t *rs);

/**
 * @brief RSAES-PKCS1-v1_5 encryption (RFC 8017 section 7.2) of `msg` under (n, e): TLS 1.2's RSA
 *        key exchange. Not for new designs.
 * @param random as many unpredictable bytes as the modulus has, from a cryptographic generator;
 *        the padding is made from them.
 * @param out the modulus's size or more; `*len` receives it.
 * @return PROVEN_ERR_INVALID_ARG for a message longer than the modulus less 11, a short `random`
 *         or a modulus outside 2048..8192 bits; PROVEN_ERR_OUT_OF_BOUNDS.
 */
[[nodiscard]]
proven_err_t proven_rsa_encrypt_pkcs1(proven_mem_view_t n, proven_mem_view_t e, proven_mem_view_t msg, proven_mem_view_t random,
                                      proven_mem_mut_t out, proven_size_t *len);

#endif /* PROVEN_SIGN_H */
