#ifndef PROVEN_ARENA_H
#define PROVEN_ARENA_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/align.h"
#include "proven/allocator.h"
#include "proven/panic.h"

/**
 * @file arena.h
 * @brief Region-based Bump Allocator implementation.
 */

/**
 * @brief Memory arena structure.
 * Statefully tracks memory allocation within a backing slice.
 */
typedef struct {
    proven_mem_mut_t backing;
    proven_size_t    offset;
} proven_arena_t;

/** Default memory alignment (typically 8 for 64-bit systems) */

/**
 * @brief Initializes an arena with a backing memory slice.
 */
static inline proven_arena_t proven_arena_create(proven_mem_mut_t backing) {
    return (proven_arena_t){ .backing = backing, .offset = 0 };
}

/**
 * @brief Resets the arena, discarding all previous allocations instantly.
 * Memory is not partially freed; it is all-or-nothing.
 */
static inline void proven_arena_reset(proven_arena_t *arena) {
    arena->offset = 0;
}

/**
 * @brief Formal cleanup for arenas. 
 * Note: If the arena was created via a manual backing slice, this is a No-Op.
 */
static inline void proven_arena_destroy(proven_arena_t *arena) {
    (void)arena;
}

/**
 * @brief Allocates memory from the arena with a specific alignment.
 * 
 * @param arena Pointer to the arena state.
 * @param size Number of bytes to allocate.
 * @param align Alignment boundary (must be a power of 2).
 * @return A result containing the allocated mutable slice or an error.
 */
[[nodiscard]]
proven_result_mem_mut_t proven_arena_alloc_aligned(proven_arena_t *arena, proven_size_t size, proven_size_t align);

/**
 * @brief Attempts an in-place tail reallocation when the allocation is the
 * most recent arena allocation.
 */
[[nodiscard]]
proven_result_mem_mut_t proven_arena_realloc_aligned(proven_arena_t *arena, void *old_ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align);

/**
 * @brief Allocates memory from the arena using the default alignment.
 */
[[nodiscard]]
static inline proven_result_mem_mut_t proven_arena_alloc(proven_arena_t *arena, proven_size_t size) {
    return proven_arena_alloc_aligned(arena, size, PROVEN_DEFAULT_ALIGNMENT);
}

/**
 * @brief Allocates memory from the arena with a specific alignment, panicking on failure.
 *
 * Production panic handlers should not return.
 * Test panic handlers may return only when intentionally verifying panic paths.
 * If a panic handler returns, the returned memory slice is not guaranteed to be valid.
 */
[[nodiscard]]
static inline proven_mem_mut_t proven_arena_alloc_aligned_or_panic(proven_arena_t *arena, proven_size_t size, proven_size_t align) {
    proven_result_mem_mut_t res = proven_arena_alloc_aligned(arena, size, align);
    if (res.err != PROVEN_OK) {
        proven_panic("proven_arena_alloc_aligned_or_panic: Out of Memory");
    }
    return res.value;
}

/**
 * @brief Allocates memory from the arena using the default alignment, panicking on failure.
 *
 * Production panic handlers should not return.
 * Test panic handlers may return only when intentionally verifying panic paths.
 * If a panic handler returns, the returned memory slice is not guaranteed to be valid.
 */
[[nodiscard]]
static inline proven_mem_mut_t proven_arena_alloc_or_panic(proven_arena_t *arena, proven_size_t size) {
    return proven_arena_alloc_aligned_or_panic(arena, size, PROVEN_DEFAULT_ALIGNMENT);
}

/**
 * @brief Allocator interface injection wrapper for Arena
 */
static inline proven_result_mem_mut_t proven_arena_alloc_trait(void *ctx, proven_size_t size, proven_size_t align) {
    return proven_arena_alloc_aligned((proven_arena_t*)ctx, size, align);
}

/**
 * @brief Reallocator zero-copy injection wrapper for Arena
 */
static inline proven_result_mem_mut_t proven_arena_realloc_trait(void *ctx, void *old_ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align) {
    return proven_arena_realloc_aligned((proven_arena_t*)ctx, old_ptr, old_size, new_size, align);
}

/**
 * @brief Arena free trait (No-Op as partial free is forbidden).
 */
static inline void proven_arena_free_trait(void *ctx, void *ptr) {
    (void)ctx;
    (void)ptr;
}

/**
 * @brief Creates a polymorphic allocator trait bound to the current Arena.
 */
static inline proven_allocator_t proven_arena_as_allocator(proven_arena_t *arena) {
    if (!arena) {
        return (proven_allocator_t){0};
    }
    return (proven_allocator_t){ 
        .ctx = arena, 
        .alloc_fn = proven_arena_alloc_trait,
        .realloc_fn = proven_arena_realloc_trait,
        .free_fn = proven_arena_free_trait
    };
}

#endif /* PROVEN_ARENA_H */
