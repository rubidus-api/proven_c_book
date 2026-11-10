#ifndef PROVEN_SYS_CHACHA_H
#define PROVEN_SYS_CHACHA_H

/* ChaCha20 (RFC 8439) several blocks at a time with the processor's vector instructions: on x86,
 * 64-bit and 32-bit, four blocks with SSE2 and eight with AVX2. Detection is at run time.
 * Everything else, and every freestanding build, reports 0 and is never called. Only the
 * keystream and Poly1305's bulk blocks are here; the cipher, the rest of the MAC and the AEAD
 * are in src/proven/crypto_chacha.c. */

#include <stddef.h>
#include <stdint.h>

/* How many blocks one call does on this processor: 8, 4, or 0 when there is no vector path. */
int proven_sys_chacha_width(void);

/* XOR up to `len` bytes of `in` with the keystream of `state` into `out` (they may be the same),
 * in groups of `width` blocks (4 or 8, at most proven_sys_chacha_width()) and then of four.
 * `state` is the sixteen words of RFC 8439 section 2.3; word 12, the block counter, counts up
 * from its value and wraps at 2^32 as the portable code does. Returns how many bytes it did: all
 * of `len`, or a multiple of 64 that leaves at most two blocks for the caller. */
size_t proven_sys_chacha20_xor(const uint32_t state[16], const uint8_t *in, uint8_t *out, size_t len, int width);

/* Poly1305 (RFC 8439 section 2.5) over `blocks` whole 16-byte blocks of `m`, each with its 2^128
 * bit, four at a time: h = (h + m_1) r^n + m_2 r^(n-1) + ... + m_n r mod 2^130 - 5. `h` and `r` are
 * five 26-bit limbs (h may be partly reduced, each limb under 2^27; r clamped). `blocks` is a
 * multiple of four, at least four. Only where proven_sys_chacha_width() is 8 (AVX2). */
void proven_sys_poly1305_blocks(uint32_t h[5], const uint32_t r[5], const uint8_t *m, size_t blocks);

#endif
