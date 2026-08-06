#ifndef PROVEN_PLATFORM_SYS_MEM_H
#define PROVEN_PLATFORM_SYS_MEM_H

#include "proven/types.h"

/**
 * @file proven_sys_mem.h
 * @brief Platform Abstraction Layer (PAL) for system memory allocation APIs.
 * This is the ONLY boundary where system or OS <stdlib.h> headers are permitted.
 *
 * NOTE: For all memory manipulation functions (copy, move, zero, set), if 
 * size > 0 and any required pointer is NULL, the operation is skipped.
 * Callers should ensure pointers are valid if failure to operate is a logic error.
 */

#ifdef PROVEN_FREESTANDING

static inline void* proven_sys_mem_alloc(proven_size_t size, proven_size_t align) {
    (void)size; (void)align;
    return (void*)0;
}

static inline void* proven_sys_mem_realloc(void* ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align) {
    (void)ptr; (void)old_size; (void)new_size; (void)align;
    return (void*)0;
}

static inline void proven_sys_mem_free(void* ptr) {
    (void)ptr;
}

static inline void proven_sys_mem_copy(void* dst, const void* src, proven_size_t size) {
    if (size > 0 && dst && src) {
        char *d = (char *)dst;
        const char *s = (const char *)src;
        for (proven_size_t i = 0; i < size; ++i) d[i] = s[i];
    }
}

static inline void proven_sys_mem_move(void* dst, const void* src, proven_size_t size) {
    if (size > 0 && dst && src && dst != src) {
        char *d = (char *)dst;
        const char *s = (const char *)src;
        if (d < s) {
            for (proven_size_t i = 0; i < size; ++i) d[i] = s[i];
        } else {
            for (proven_size_t i = size; i > 0; --i) d[i - 1] = s[i - 1];
        }
    }
}

static inline void proven_sys_mem_zero(void* dst, proven_size_t size) {
    if (size > 0 && dst) {
        char *d = (char *)dst;
        for (proven_size_t i = 0; i < size; ++i) d[i] = 0;
    }
}

static inline void proven_sys_mem_set(void* dst, int value, proven_size_t size) {
    if (size > 0 && dst) {
        char *d = (char *)dst;
        char v = (char)value;
        for (proven_size_t i = 0; i < size; ++i) d[i] = v;
    }
}

static inline int proven_sys_mem_cmp(const void* s1, const void* s2, proven_size_t size) {
    if (size == 0) return 0;
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    for (proven_size_t i = 0; i < size; ++i) {
        if (p1[i] != p2[i]) return (int)p1[i] - (int)p2[i];
    }
    return 0;
}

static inline const void* proven_sys_mem_chr(const void* s, int c, proven_size_t n) {
    if (!s || n == 0) return (const void*)0;
    const unsigned char *p = (const unsigned char *)s;
    unsigned char target = (unsigned char)c;
    /* SWAR word-at-a-time scan; the 8-byte load is built by byte copies so there
       is no alignment or strict-aliasing UB, and the wide stride only runs while
       at least 8 bytes remain so it never over-reads. */
    const proven_u64 ones = (proven_u64)0x0101010101010101ull;
    const proven_u64 highs = (proven_u64)0x8080808080808080ull;
    proven_u64 cmask = ones * (proven_u64)target;
    while (n >= 8u) {
        proven_u64 w = 0;
        unsigned char *wp = (unsigned char *)&w;
        for (int i = 0; i < 8; ++i) wp[i] = p[i];
        proven_u64 x = w ^ cmask;
        if (((x - ones) & ~x & highs) != 0u) {
            for (proven_size_t i = 0; i < 8u; ++i) {
                if (p[i] == target) return (const void*)(p + i);
            }
        }
        p += 8u;
        n -= 8u;
    }
    while (n > 0u) {
        if (*p == target) return (const void*)p;
        ++p;
        --n;
    }
    return (const void*)0;
}

#else

/**
 * @brief Allocates an aligned chunk of raw memory from the external system (e.g., OS Heap).
 */
[[nodiscard]]
void* proven_sys_mem_alloc(proven_size_t size, proven_size_t align);

/**
 * @brief Reallocates a block, growing it in place where the allocator can.
 *
 * @note `old_size` and `align` must be the ones the block was allocated with.
 *       Requests at or below alignof(max_align_t) are served by malloc/realloc,
 *       so they can grow in place (for large blocks that means remapping pages
 *       rather than copying them); over-aligned requests use the aligned family
 *       and are moved by hand. A block must not cross between the two families.
 * @note `new_size == 0` frees the block and returns NULL.
 * @note Failure atomicity: on failure the old block is left untouched.
 */
[[nodiscard]]
void* proven_sys_mem_realloc(void* ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align);

/**
 * @brief Frees memory allocated by proven_sys_mem_alloc.
 */
void proven_sys_mem_free(void* ptr);

/**
 * @brief Copies memory from source to destination using the system routine.
 * Behavior is undefined if source and destination regions overlap.
 */
void proven_sys_mem_copy(void* dst, const void* src, proven_size_t size);

/**
 * @brief Moves memory from source to destination safely allowing for overlapping regions.
 */
void proven_sys_mem_move(void* dst, const void* src, proven_size_t size);

/**
 * @brief Zeroes out a region of memory.
 */
void proven_sys_mem_zero(void* dst, proven_size_t size);

/**
 * @brief Sets a region of memory to a specific byte value.
 */
void proven_sys_mem_set(void* dst, int value, proven_size_t size);

/**
 * @brief Compares memory between two regions.
 */
int proven_sys_mem_cmp(const void* s1, const void* s2, proven_size_t size);

/**
 * @brief Finds the first byte equal to `c` in the first `n` bytes of `s`.
 * Returns a pointer to it, or NULL. Uses the system routine when hosted.
 */
const void* proven_sys_mem_chr(const void* s, int c, proven_size_t n);

#endif /* PROVEN_FREESTANDING */

#endif /* PROVEN_PLATFORM_SYS_MEM_H */
