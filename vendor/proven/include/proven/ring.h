#ifndef PROVEN_RING_H
#define PROVEN_RING_H

#include <stdalign.h>
#include "proven/types.h"
#include "proven/error.h"
#include "proven/allocator.h"
#include "proven/align.h"

/**
 * @file ring.h
 * @brief Fixed-capacity Circular Ring Buffer.
 *        Push fails when the buffer is full; pop succeeds or returns empty.
 */

typedef struct {
    proven_allocator_t alloc;    // Backing trait allocator
    proven_mem_mut_t internal;   // Fixed continuous byte block array
    proven_size_t head;          // Read cursor index mathematically wrapping boundary
    proven_size_t tail;          // Write cursor index mathematically wrapping boundary
    proven_size_t len;           // Precise element count populated correctly
    proven_size_t cap;           // Constant exact capability limit
    proven_size_t elem_size;     // Struct size footprint
    proven_size_t align;         // Struct memory alignment
} proven_ring_t;

typedef struct {
    proven_err_t err;
    proven_ring_t value;
} proven_result_ring_t;

// -------------------------------------------------------------
// Type-Agnostic Core C API
// -------------------------------------------------------------

/**
 * @brief Create a fixed-capacity ring of `cap` elements, each `elem_size` bytes.
 *
 * @param align Element alignment; must be a power of two. For the elements to actually land
 *              on `align`-aligned offsets, `elem_size` must be a multiple of `align` - this
 *              is not checked, because internally only memcpy touches elements (which has no
 *              alignment requirement). The PROVEN_RING_INIT macro always threads a consistent
 *              sizeof(type)/alignof(type), which satisfies this; a caller of the raw API with
 *              mismatched values gets a working ring whose element offsets are not `align`-aligned.
 */
[[nodiscard]] proven_result_ring_t proven_ring_create(proven_allocator_t alloc, proven_size_t cap, proven_size_t elem_size, proven_size_t align);

/**
 * @brief Validates the structural integrity of the public ring fields.
 */
[[nodiscard]] bool proven_ring_is_valid(const proven_ring_t *ring);

[[nodiscard]] proven_err_t proven_ring_push(proven_ring_t *ring, const void *element);

/**
 * @brief Pops one element from the ring.
 *
 * If out_element is NULL, the element is discarded.
 */
[[nodiscard]] proven_err_t proven_ring_pop(proven_ring_t *ring, void *out_element);

void proven_ring_destroy(proven_ring_t *ring);

// -------------------------------------------------------------
// Type-Safe Strict Macro Wrappers
// -------------------------------------------------------------

/**
 * @brief Generates fixed boundary ring allocating memory constraints directly bypassing external pointer logic.
 */
#define PROVEN_RING_INIT(alloc, type, cap) \
    proven_ring_create((alloc), (cap), sizeof(type), alignof(type))

/**
 * @brief Thread-independent value injection; fails when the buffer is full.
 */
#define PROVEN_RING_PUSH(ring_ptr, type, value) \
    proven_ring_push((ring_ptr), (type[]){(value)})

/**
 * @brief Safely pops frontmost data wrapping indexes. Target pointer accepts valid type structurally explicitly.
 */
#define PROVEN_RING_POP(ring_ptr, type, out_ptr) \
    proven_ring_pop((ring_ptr), (out_ptr))

/**
 * @brief Destroys ring structures cascading memory releases polymorphically toward internal Allocator parameters natively.
 */
#define PROVEN_RING_DESTROY(ring_ptr) \
    proven_ring_destroy(ring_ptr)

#endif /* PROVEN_RING_H */
