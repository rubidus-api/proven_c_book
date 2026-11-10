#ifndef PROVEN_KEX_H
#define PROVEN_KEX_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/sign.h"

/**
 * @file kex.h
 * @brief Key exchange: X25519 (RFC 7748), ECDH over P-256 and P-384 (SEC 1, NIST SP 800-56A),
 *        and finite-field Diffie-Hellman over six fixed groups (RFC 7919, RFC 3526). Since
 *        v0.111.0; the code is the one the TLS engine agrees its keys with.
 *
 * Each side makes a private value from a cryptographic generator, sends the public value made
 * from it, and computes the shared secret from its own private value and the peer's public one.
 * The shared secret is not a key: feed it to a KDF (proven_hkdf_extract). Use a private value for
 * one exchange only.
 *
 * Secrets: private values and shared secrets are SECRET; the code takes the same time whatever
 * they are. Peer values are checked: a point not on the curve, an X25519 result of all zeros (a
 * point of small order) and a DH value outside 2 .. p-2 are refused, since each would give a
 * shared secret an attacker can name.
 *
 * Pure computation: no allocation, no OS; part of the freestanding profile.
 */

// -------------------------------------------------------------
// X25519
// -------------------------------------------------------------

/** @brief The public value of a 32-byte private scalar (random bytes; clamped as RFC 7748 says). */
void proven_x25519_public(proven_byte_t out[32], const proven_byte_t scalar[32]);

/**
 * @brief The shared secret of a private scalar and the peer's public value.
 * @return PROVEN_OK; PROVEN_ERR_UNTRUSTED when the result is all zeros (the peer sent a point of
 *         small order) - `out` then holds nothing to use.
 */
[[nodiscard]]
proven_err_t proven_x25519(proven_byte_t out[32], const proven_byte_t scalar[32], const proven_byte_t peer[32]);

// -------------------------------------------------------------
// ECDH over P-256 and P-384
// -------------------------------------------------------------

/**
 * @brief The public point (uncompressed: 04 | X | Y, 65 or 97 bytes) of a private scalar
 *        (big-endian, the curve's size: 32 or 48 bytes).
 * @param curve PROVEN_PKEY_P256 or PROVEN_PKEY_P384.
 * @return PROVEN_ERR_INVALID_FORMAT when the scalar is zero or not below the group's order;
 *         PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small; PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_ec_public(proven_pkey_kind_t curve, proven_mem_view_t scalar, proven_mem_mut_t out);

/**
 * @brief The shared secret - the X coordinate of scalar times the peer's point, the curve's size
 *        in bytes - into `out`.
 * @return PROVEN_ERR_INVALID_FORMAT for a peer point that is not an uncompressed point of the
 *         curve, or a scalar out of range; PROVEN_ERR_OUT_OF_BOUNDS; PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_ecdh(proven_pkey_kind_t curve, proven_mem_view_t scalar, proven_mem_view_t peer, proven_mem_mut_t out);

// -------------------------------------------------------------
// Finite-field Diffie-Hellman over known groups
// -------------------------------------------------------------

/**
 * Any prime a peer names is not examined: a group may be too small, not prime, or have a
 * subgroup that gives the exponent away, and none of that can be checked in the time an exchange
 * takes. So only six primes are used, each a safe prime with generator 2: ffdhe2048, ffdhe3072
 * and ffdhe4096 (RFC 7919) and the 2048-, 3072- and 4096-bit MODP groups (RFC 3526).
 */

#define PROVEN_DH_MAX_BYTES     ((proven_size_t)512)   /**< the largest prime, 4096 bits */
#define PROVEN_DH_EXP_MAX_BYTES ((proven_size_t)44)    /**< the longest private exponent */

/** @brief ffdhe2048's prime (RFC 7919), 256 bytes; its generator is 2. */
[[nodiscard]]
proven_mem_view_t proven_dh_ffdhe2048(void);

/** @brief Whether `p` (leading zeros allowed) is one of the six primes and `g` is 2. */
[[nodiscard]]
bool proven_dh_known(proven_mem_view_t p, proven_mem_view_t g);

/** @brief How many random bytes make a private exponent for a prime of `p_len` bytes: 32, 36 or 44. */
[[nodiscard]]
proven_size_t proven_dh_exponent_len(proven_size_t p_len);

/**
 * @brief 2^priv mod p into `out`, the prime's size in bytes.
 * @param priv the private exponent: random bytes, a multiple of 4 in length, up to
 *        PROVEN_DH_EXP_MAX_BYTES (proven_dh_exponent_len says how many).
 * @return PROVEN_ERR_UNSUPPORTED for a prime that is not one of the six; PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_dh_public(proven_mem_view_t p, proven_mem_view_t priv, proven_byte_t *out);

/**
 * @brief peer^priv mod p, with its leading zero bytes dropped as TLS 1.2 wants its premaster
 *        (RFC 5246 section 8.1.2): up to the prime's size into `out`, the length in `*out_len`.
 *        Because the length shows how many were dropped, never use a private exponent twice.
 * @return PROVEN_ERR_INVALID_FORMAT for a peer value outside 2 .. p-2; PROVEN_ERR_UNSUPPORTED for
 *         an unknown prime; PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_dh_shared(proven_mem_view_t p, proven_mem_view_t priv, proven_mem_view_t peer, proven_byte_t *out, proven_size_t *out_len);

#endif /* PROVEN_KEX_H */
