#ifndef PROVEN_SYS_RANDOM_CHUNK_H
#define PROVEN_SYS_RANDOM_CHUNK_H

#include <stddef.h>

/*
 * RFC-0006 H-006. The Windows entropy source is BCryptGenRandom, whose length argument is
 * a ULONG. proven_sys_random_bytes takes a size_t, and it used to cast the whole length to
 * ULONG once. On 64-bit Windows a length above ULONG_MAX narrows silently: only the low 32
 * bits are requested - possibly zero bytes - and STATUS_SUCCESS for that short request is
 * then returned as success for the whole buffer. The caller reads the untouched remainder
 * as fresh entropy, which is the worst way for a random-number call to fail.
 *
 * The planner is here, in its own header, so it can be checked at the boundaries without
 * allocating gigabytes and without constructing a pointer outside any real object: a test
 * passes a reduced artificial limit and inspects the sizes it asks for.
 */

/**
 * @brief How many bytes the next OS call should ask for.
 *
 * @param remaining bytes still to fill; 0 means there is nothing left to do.
 * @param limit     the largest count the backend accepts in one call. Must be nonzero.
 * @return `remaining` when it fits, otherwise `limit`. Never more than either.
 */
static inline size_t proven_sys_random_chunk(size_t remaining, size_t limit) {
    if (limit == 0) return 0;
    return (remaining > limit) ? limit : remaining;
}

#endif /* PROVEN_SYS_RANDOM_CHUNK_H */
