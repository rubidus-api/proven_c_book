#ifndef PROVEN_ALLOCATOR_H
#define PROVEN_ALLOCATOR_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"

/**
 * @file allocator.h
 * @brief Polymorphic memory allocation interface (Trait).
 */

/**
 * @brief Function signature for a raw memory allocation trait.
 *
 * @note `align` must be a power of two.
 * @note A block must be reallocated with the SAME `align` it was allocated with,
 *       and freed through the same allocator. An allocator is entitled to pick a
 *       different underlying mechanism for over-aligned requests than for
 *       ordinary ones - the heap allocator does exactly that, using malloc and
 *       realloc when `align <= alignof(max_align_t)` (which is every string,
 *       buffer and byte array in this library) so that growth can happen in
 *       place, and an aligned allocator otherwise. Handing a block back with a
 *       different alignment class is undefined.
 *
 * @note `size == 0` is PROVEN_ERR_INVALID_ARG. A zero-byte allocation is a caller bug,
 *       and the two allocators used to disagree about it - the heap said NOMEM (a lie:
 *       nothing was out of memory) and the arena handed back a live pointer to nothing.
 *       Trait-generic code cannot be written against a rule that changes per allocator.
 */
typedef proven_result_mem_mut_t (*proven_alloc_fn_t)(void *ctx, proven_size_t size, proven_size_t align);

/**
 * @brief Function signature for reallocating memory through the trait.
 *
 * @note `old_size` and `align` must be the ones the block was allocated with.
 * @note `new_size == 0` frees the block and returns a null pointer with
 *       PROVEN_OK.
 * @note Failure atomicity: on failure `old_ptr` must be left valid and
 *       unmodified, so the caller still owns the original block.
 */
typedef proven_result_mem_mut_t (*proven_realloc_fn_t)(void *ctx, void *old_ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align);

/**
 * @brief Function signature for deallocating memory through the trait.
 */
typedef void (*proven_free_fn_t)(void *ctx, void *ptr);

/**
 * @brief An interface wrapper allowing functions to be allocator-agnostic 
 * (Supports both Arena and Malloc-style Heap dynamically).
 */
typedef struct {
    void *ctx;
    proven_alloc_fn_t alloc_fn;
    /**
     * @brief Must leave old_ptr valid and unmodified on failure.
     */
    proven_realloc_fn_t realloc_fn;
    proven_free_fn_t  free_fn;
} proven_allocator_t;

/**
 * @brief Checks if all allocator functions are correctly provided.
 */
static inline bool proven_alloc_is_valid(proven_allocator_t alloc) {
    return alloc.alloc_fn && alloc.realloc_fn && alloc.free_fn;
}

#endif /* PROVEN_ALLOCATOR_H */
