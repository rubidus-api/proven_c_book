#ifndef PROVEN_SYS_RANDOM_H
#define PROVEN_SYS_RANDOM_H

/**
 * @file proven_sys_random.h
 * @brief PAL: cryptographically strong random bytes from the OS CSPRNG.
 */

#include "proven/types.h"

/**
 * @brief Fill buf with len strong random bytes. Returns false if no CSPRNG is available
 *        (freestanding) or the OS call failed. len == 0 is a successful no-op.
 */
[[nodiscard]]
bool proven_sys_random_bytes(void *buf, size_t len);

/**
 * @brief The fork generation: a count that goes up by one in the child of every fork. POSIX
 *        through `pthread_atfork` (a child handler registered by the first call of this
 *        function); always 0 on Windows, which has no fork. A raw `clone` or `vfork` that
 *        bypasses the C library's fork is not seen. For proven_chacha_rng_t (RFC-0048).
 * @return the generation now.
 */
unsigned proven_sys_fork_watch(void);

/** @brief The fork generation now, without registering anything: a relaxed atomic load. */
unsigned proven_sys_fork_generation(void);
/** @brief The count itself, for a load on every draw without a call (it stays 0 on Windows). */
extern _Atomic unsigned proven_sys_fork_generation_;

/** @brief This process's id, for a key a forked child derives when it gets no entropy. */
unsigned proven_sys_process_id(void);

/**
 * @brief How many ChaCha20 blocks proven_sys_chacha_keystream does at once on this processor:
 *        8 (AVX2), 4 (SSE2), or 0 when there is no vector path (then it is never called).
 *        Detected once, at the first call. x86, 64-bit and 32-bit, with GCC or Clang.
 */
int proven_sys_chacha_keystream_width(void);

/**
 * @brief Write the keystream of `blocks` consecutive ChaCha20 blocks (RFC 8439 section 2.3) of
 *        `state` to `out`: 64 bytes each, block k with word 12 at state[12] + k. `blocks` is a
 *        multiple of four, at least four; groups of eight where the width is 8. The caller keeps
 *        word 12 from wrapping inside one call. For proven_chacha_rng_fill (src/proven/random.c),
 *        whose portable block function this matches byte for byte. Copies of the keystream left
 *        on the stack are zeroed.
 */
void proven_sys_chacha_keystream(const uint32_t state[16], uint8_t *out, size_t blocks);

/**
 * @brief One block of the same keystream (word 12 as it is in `state`) into `out`, 64 bytes, on
 *        SSE2's four lanes as the block's four rows. Only where the width is not 0.
 */
void proven_sys_chacha_keystream1(const uint32_t state[16], uint8_t *out);

#endif /* PROVEN_SYS_RANDOM_H */
