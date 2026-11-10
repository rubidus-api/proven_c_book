#ifndef PROVEN_AEAD_H
#define PROVEN_AEAD_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"

/**
 * @file aead.h
 * @brief Authenticated encryption: AES-GCM (NIST SP 800-38D, 96-bit nonces) and
 *        ChaCha20-Poly1305 (RFC 8439); the raw ChaCha20 stream and one AES block, which QUIC's
 *        header protection needs (RFC 9001 section 5.4). Since v0.111.0; the code is the one the
 *        TLS and QUIC engines encrypt their records with.
 *
 * An AEAD encrypts and authenticates in one step: open refuses any ciphertext, tag, nonce or
 * associated data that was changed, and gives out nothing in that case. A nonce must never be
 * used twice under one key - doing so gives away the authentication key (GCM) or the XOR of two
 * plaintexts (both). Protocols derive nonces from a sequence number for this reason.
 *
 * Secrets: keys, contexts and plaintexts are SECRET. No time here depends on them: AES uses the
 * processor's AES and carry-less multiply instructions where it has them (chosen at run time),
 * otherwise a bitsliced cipher with no table look-ups; ChaCha20 and Poly1305 are arithmetic
 * only. Tags are compared in constant time. Wipe a context with proven_mem_wipe when done.
 *
 * Pure computation: no allocation, no OS; part of the freestanding profile (the portable AES
 * there).
 */

#define PROVEN_AEAD_NONCE_SIZE ((proven_size_t)12)
#define PROVEN_AEAD_TAG_SIZE   ((proven_size_t)16)

/** @brief An AES-GCM key, prepared: round keys and the GHASH key. The fields are private. */
typedef struct {
    int rounds;
    bool hw;
    bool one_pass;
    proven_byte_t rk[240];
    proven_byte_t h[16];
    proven_byte_t hpow[112];
} proven_aes_gcm_t;

/** @brief An AES key for the block cipher alone (no GHASH key). The fields are private. */
typedef struct {
    int rounds;
    bool hw;
    proven_byte_t rk[240];
} proven_aes_t;

/** @brief Prepare an AES-GCM key of 16 or 32 bytes. PROVEN_ERR_INVALID_ARG otherwise. */
[[nodiscard]]
proven_err_t proven_aes_gcm_init(proven_aes_gcm_t *ctx, proven_mem_view_t key);

/**
 * @brief Encrypt `plain` into `out` (plain.size bytes; `out` may be `plain.ptr`) and write the
 *        16-byte tag over it and `aad`.
 */
void proven_aes_gcm_seal(const proven_aes_gcm_t *ctx, const proven_byte_t nonce[12], proven_mem_view_t aad,
                         proven_mem_view_t plain, proven_byte_t *out, proven_byte_t tag[16]);

/**
 * @brief Check the tag and decrypt `cipher` into `out` (cipher.size bytes; may be `cipher.ptr`).
 * @return PROVEN_OK; PROVEN_ERR_UNTRUSTED when the tag does not match - `out` is then zeroed,
 *         never left holding unauthenticated text.
 */
[[nodiscard]]
proven_err_t proven_aes_gcm_open(const proven_aes_gcm_t *ctx, const proven_byte_t nonce[12], proven_mem_view_t aad,
                                 proven_mem_view_t cipher, const proven_byte_t tag[16], proven_byte_t *out);

/** @brief ChaCha20-Poly1305 sealing (RFC 8439 section 2.8), as proven_aes_gcm_seal. */
void proven_chacha20_poly1305_seal(const proven_byte_t key[32], const proven_byte_t nonce[12], proven_mem_view_t aad,
                                   proven_mem_view_t plain, proven_byte_t *out, proven_byte_t tag[16]);

/** @brief ChaCha20-Poly1305 opening, as proven_aes_gcm_open. */
[[nodiscard]]
proven_err_t proven_chacha20_poly1305_open(const proven_byte_t key[32], const proven_byte_t nonce[12], proven_mem_view_t aad,
                                           proven_mem_view_t cipher, const proven_byte_t tag[16], proven_byte_t *out);

/**
 * @brief XOR `len` bytes of ChaCha20 keystream (RFC 8439 section 2.4) into `out` from `in` (they
 *        may be the same), starting at block `counter`. Not authenticated: for QUIC's header
 *        protection and other constructions that authenticate elsewhere.
 */
void proven_chacha20(const proven_byte_t key[32], proven_u32 counter, const proven_byte_t nonce[12],
                     const proven_byte_t *in, proven_byte_t *out, proven_size_t len);

/** @brief Prepare an AES key of 16 or 32 bytes for proven_aes_encrypt_block. */
[[nodiscard]]
proven_err_t proven_aes_init(proven_aes_t *ctx, proven_mem_view_t key);

/** @brief Encrypt one 16-byte block (FIPS 197); `in` and `out` may be the same. */
void proven_aes_encrypt_block(const proven_aes_t *ctx, const proven_byte_t in[16], proven_byte_t out[16]);

/** @brief Whether a key prepared now uses the processor's AES instructions. */
[[nodiscard]]
bool proven_aes_hw_available(void);

#endif /* PROVEN_AEAD_H */
