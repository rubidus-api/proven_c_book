#ifndef PROVEN_ALIGN_H
#define PROVEN_ALIGN_H

#include "types.h"
#include <stdalign.h>

/**
 * @file align.h
 * @brief Memory alignment utilities.
 */

#ifndef PROVEN_DEFAULT_ALIGNMENT
#define PROVEN_DEFAULT_ALIGNMENT ((proven_size_t)8)
#endif

// Matches the alignment of max_align_t, usually 16 on modern systems
#define PROVEN_MAX_ALIGN alignof(max_align_t)

static inline bool proven_is_pow2(proven_size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

/**
 * @brief Aligns an address up to the nearest multiple of align.
 * 
 * @param addr The address or size to align.
 * @param align The alignment boundary. MUST be a power of 2.
 * @return The aligned address, or 0 if overflow occurs or align is not power of 2.
 */
static inline proven_size_t proven_mem_align_up(proven_size_t addr, proven_size_t align) {
    if (!proven_is_pow2(align)) return 0;
    proven_size_t mask = align - 1;
    proven_size_t res;
    if (PROVEN_CKD_ADD(&res, addr, mask)) {
        return 0; // Overflow
    }
    return res & ~mask;
}

/**
 * @brief Aligns a uintptr_t address up to the nearest multiple of align.
 * 
 * @param addr The address to align.
 * @param align The alignment boundary. MUST be a power of 2.
 * @return The aligned address, or 0 if overflow occurs or align is not power of 2.
 */
static inline proven_uintptr_t proven_uintptr_align_up(proven_uintptr_t addr, proven_size_t align) {
    if (!proven_is_pow2(align)) return 0;
    proven_uintptr_t mask = (proven_uintptr_t)align - 1;
    proven_uintptr_t res;
    if (PROVEN_CKD_ADD(&res, addr, mask)) {
        return 0; // Overflow
    }
    return res & ~mask;
}

#endif /* PROVEN_ALIGN_H */
