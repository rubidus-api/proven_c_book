#ifndef PROVEN_HEAP_H
#define PROVEN_HEAP_H

#include "proven/allocator.h"

/**
 * @file heap.h
 * @brief Standard malloc-style heap allocator wrapped into the proven_allocator_t trait.
 */

/**
 * @brief Returns an allocator trait powered by the system's underlying Heap (malloc).
 * 
 * @note Under PROVEN_FREESTANDING, this returns a zero-initialized `(proven_allocator_t){0}` stub.
 * `proven_alloc_is_valid()` will subsequently return false for the returned allocator.
 */
[[nodiscard]]
proven_allocator_t proven_heap_allocator(void);

#endif /* PROVEN_HEAP_H */
