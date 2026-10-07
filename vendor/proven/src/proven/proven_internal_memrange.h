#ifndef PROVEN_INTERNAL_MEMRANGE_H
#define PROVEN_INTERNAL_MEMRANGE_H

#include <stdbool.h>
#include "proven/memory.h"

typedef struct proven_bufref_t {
    bool valid;
    proven_size_t offset;
    proven_size_t size;
} proven_bufref_t;

/**
 * @brief Captures a pointer range relative to a base buffer.
 * Follows proven_range_contains_ptr() semantics, including accepting
 * a one-past pointer for an empty range (size == 0).
 */
static inline proven_bufref_t proven_bufref_capture(
    const void *base,
    proven_size_t cap,
    const void *ptr,
    proven_size_t size)
{
    proven_bufref_t ref = { .valid = false, .offset = 0, .size = size };
    if (proven_range_contains_ptr(base, cap, ptr, size, &ref.offset)) {
        ref.valid = true;
    }
    return ref;
}

static inline void *proven_bufref_rebase_mut(
    proven_bufref_t ref,
    void *new_base)
{
    if (!ref.valid || !new_base) return NULL;
    return (void *)((proven_byte_t *)new_base + ref.offset);
}

static inline const void *proven_bufref_rebase_const(
    proven_bufref_t ref,
    const void *new_base)
{
    if (!ref.valid || !new_base) return NULL;
    return (const void *)((const proven_byte_t *)new_base + ref.offset);
}

/**
 * @brief Whether [ptr, ptr + size) shares any byte with [base, base + cap).
 *
 * The question to ask before growing a buffer with input that may live inside it: a realloc can
 * move or free the storage, and ANY overlap - not only an input that starts inside - leaves the
 * input dangling. Empty ranges share nothing. Integer comparison, checked arithmetic: the two
 * ranges may belong to different objects.
 */
static inline bool proven_range_overlaps(const void *base, proven_size_t cap, const void *ptr, proven_size_t size) {
    if (!base || !ptr || cap == 0 || size == 0) return false;
    proven_uintptr_t b = (proven_uintptr_t)base, p = (proven_uintptr_t)ptr, b_end, p_end;
    if (PROVEN_CKD_ADD(&b_end, b, cap)) b_end = (proven_uintptr_t)-1;
    if (PROVEN_CKD_ADD(&p_end, p, size)) p_end = (proven_uintptr_t)-1;
    return p < b_end && b < p_end;
}

#endif // PROVEN_INTERNAL_MEMRANGE_H
