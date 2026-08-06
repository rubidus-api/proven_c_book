#ifndef PROVEN_POOL_H
#define PROVEN_POOL_H

#include "proven/config.h"
#include "proven/allocator.h"

/**
 * @brief Memory pool allocator for fixed-size objects.
 * Strictly adheres to allocating and recycling a single data type.
 */
typedef struct {
    proven_allocator_t base_alloc;
    proven_size_t item_size;
    proven_size_t item_align;
    void **bin;
    proven_size_t bin_cap;
    proven_size_t bin_len;
} proven_pool_t;

/**
 * @brief Initializes a pool allocator.
 * 
 * @param pool The pool to initialize.
 * @param base_alloc The base allocator used to allocate the bin array and new items.
 * @param item_size The exact size of the item type.
 * @param item_align The alignment of the item type.
 * @param bin_cap The capacity of the recycling bin.
 * @return PROVEN_OK on success, or an error code otherwise.
 */
[[nodiscard]]
proven_err_t proven_pool_init(proven_pool_t *pool, proven_allocator_t base_alloc, proven_size_t item_size, proven_size_t item_align, proven_size_t bin_cap);

/**
 * @brief Retrieves the generic allocator interface for this pool.
 *
 * @param pool The pool instance.
 * @return A proven_allocator_t trait wrapping the pool context.
 *
 * @note Every block returned through this trait must be freed exactly once, and only through
 *       the same pool. A double-free or a free of a foreign/misaligned pointer is a caller
 *       error: it is trapped (panic) in debug and PROVEN_HARDENED builds, but in a plain
 *       release build a double-free silently returns the block to the recycling bin twice and
 *       the pool will later hand out aliasing blocks. Free each block once; do not free
 *       pointers the pool did not give you.
 * @note The trait rejects an allocation whose requested @p align exceeds the pool's
 *       item_align. A non-power-of-two align that does not exceed item_align is accepted (the
 *       block is still item_align-aligned); passing a garbage align is a caller error per the
 *       allocator trait contract.
 */
proven_allocator_t proven_pool_as_allocator(proven_pool_t *pool);

/**
 * @brief Destroys a pool allocator, freeing all cached blocks and the bin.
 * 
 * @param pool The pool to destroy.
 */
void proven_pool_destroy(proven_pool_t *pool);

#endif /* PROVEN_POOL_H */
