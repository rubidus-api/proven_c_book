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

#endif /* PROVEN_SYS_RANDOM_H */
