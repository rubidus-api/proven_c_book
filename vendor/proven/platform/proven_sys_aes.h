#ifndef PROVEN_SYS_AES_H
#define PROVEN_SYS_AES_H

/* The processor's AES and carry-less multiply instructions, where it has them: x86, 64-bit
 * and 32-bit (AES-NI with PCLMULQDQ), and AArch64 (the ARMv8 cryptography extension).
 * Detection is at run time.
 * Everything else, and every freestanding build, reports "none" and is never called. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* True when the calls below may be used on this processor. */
bool proven_sys_aes_available(void);

/* The key schedule (FIPS 197 section 5.2) into (rounds + 1) * 16 bytes of round keys, the same
 * bytes the portable schedule makes. `key_len` 16 (rounds 10) or 32 (rounds 14). False when this
 * build has no instruction path for it (AArch64 has none yet), and nothing was written. */
bool proven_sys_aes_expand(uint8_t *rk, int rounds, const uint8_t *key, size_t key_len);

/* Encrypt one block. `rk` is (rounds + 1) * 16 bytes of round keys in the standard order. */
void proven_sys_aes_encrypt_block(const uint8_t *rk, int rounds, const uint8_t in[16], uint8_t out[16]);

/* CTR mode: XOR `len` bytes with the keystream of counter blocks iv | be32(counter),
 * be32(counter + 1), ... `in` and `out` may be the same. */
void proven_sys_aes_ctr(const uint8_t *rk, int rounds, const uint8_t iv[12], uint32_t counter,
                        const uint8_t *in, uint8_t *out, size_t len);

/* CBC decryption of `len` bytes (a multiple of 16) in place; `iv` is left holding the last
 * ciphertext block. False when this build has no instruction path for it - x86 has one,
 * AArch64 does not yet - and the caller then uses its portable code; nothing was touched. */
bool proven_sys_aes_cbc_decrypt(const uint8_t *rk, int rounds, uint8_t iv[16], uint8_t *data, size_t len);

/* GHASH: for each 16-byte block, y = (y ^ block) * h in GF(2^128) as GCM defines it. */
void proven_sys_aes_ghash(uint8_t y[16], const uint8_t h[16], const uint8_t *data, size_t blocks);

/* GCM in one pass (SP 800-38D, a 96-bit nonce): the counter mode from block 2 over `len` bytes
 * of `in` into `out` (which may be `in`), and the tag over `aad` and the ciphertext - the
 * output when sealing, the input when `decrypt`. Opening only computes the tag: comparing it,
 * and clearing `out` when it differs, is the caller's. `hpow` is H^2..H^8 as
 * proven_sys_aes_gcm_powers made them from `h`. Both return false when this build has no
 * one-pass code, and then touch nothing. */
bool proven_sys_aes_gcm_powers(const uint8_t h[16], uint8_t hpow[112]);
bool proven_sys_aes_gcm(const uint8_t *rk, int rounds, const uint8_t h[16], const uint8_t hpow[112], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aad_len, const uint8_t *in, uint8_t *out, size_t len, bool decrypt, uint8_t tag[16]);

#endif
