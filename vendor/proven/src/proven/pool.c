#include "proven/pool.h"
#include "proven/align.h"
#include "proven/panic.h"
#include "../../platform/proven_sys_mem.h"

proven_err_t proven_pool_init(proven_pool_t *pool, proven_allocator_t base_alloc, proven_size_t item_size, proven_size_t item_align, proven_size_t bin_cap) {
    if (!pool || !proven_alloc_is_valid(base_alloc) || item_size == 0 || !proven_is_pow2(item_align)) {
        return PROVEN_ERR_INVALID_ARG;
    }

    pool->base_alloc = base_alloc;
    pool->item_size = item_size;
    pool->item_align = item_align;
    pool->bin_cap = 0;
    pool->bin_len = 0;
    pool->bin = 0;

    if (bin_cap > 0) {
        proven_size_t bin_size;
        if (PROVEN_CKD_MUL(&bin_size, bin_cap, sizeof(void*))) {
            return PROVEN_ERR_NOMEM;
        }

        proven_result_mem_mut_t res = base_alloc.alloc_fn(base_alloc.ctx, bin_size, alignof(void*));
        if (!PROVEN_IS_OK(res.err)) {
            return res.err;
        }

        pool->bin = (void **)res.value.ptr;
    }

    /* Publish the capacity only once the bin behind it exists. Setting it up
     * front left a failed init claiming bin_cap slots with bin == NULL, and the
     * free trait tests bin_len < bin_cap before writing bin[bin_len] - so a pool
     * used after an ignored init error wrote through a null pointer. */
    pool->bin_cap = bin_cap;
    return PROVEN_OK;
}

static proven_result_mem_mut_t proven_pool_alloc_trait(void *ctx, proven_size_t size, proven_size_t align) {
    proven_result_mem_mut_t res = {0};
    proven_pool_t *pool = (proven_pool_t *)ctx;

    if (!pool) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (size == 0) {
        /* Same rule as every other allocator: a zero-byte allocation is a caller bug. */
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (size != pool->item_size || align > pool->item_align) {
        /* A pool serves ONE item size and one alignment. A request for anything else is
         * not nonsense - it is a perfectly sensible request that this allocator is not the
         * one to serve. It used to say PROVEN_ERR_INVALID_ARG, which reads as "you passed
         * me garbage" and sends a trait-generic caller looking for a bug in itself. */
        res.err = PROVEN_ERR_UNSUPPORTED;
        return res;
    }

    if (pool->bin_len > 0) {
        pool->bin_len--;
        res.value.ptr = pool->bin[pool->bin_len];
        res.value.size = pool->item_size;
        res.err = PROVEN_OK;
        return res;
    }

    return pool->base_alloc.alloc_fn(pool->base_alloc.ctx, size, pool->item_align);
}

static void proven_pool_free_trait(void *ctx, void *ptr);

static proven_result_mem_mut_t proven_pool_realloc_trait(void *ctx, void *old_ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align) {
    (void)old_size;
    (void)align;
    proven_result_mem_mut_t res = {0};

    /*
     * A pool manages fixed-size blocks, so it genuinely cannot grow one. It can, however,
     * answer the two requests that are not growth - and it used to refuse those too, with
     * PROVEN_ERR_INVALID_ARG, which is what a trait-generic caller reads as "you passed me
     * nonsense" rather than "I cannot do that".
     */
    if (new_size == 0) {
        /* The trait says: free the block, return a null pointer, PROVEN_OK. Every other
         * allocator does. So does this one now. */
        proven_pool_free_trait(ctx, old_ptr);
        res.err = PROVEN_OK;
        res.value.ptr = NULL;
        res.value.size = 0;
        return res;
    }

    if (!old_ptr) {
        return proven_pool_alloc_trait(ctx, new_size, align);
    }

    proven_pool_t *pool = (proven_pool_t *)ctx;
    if (pool && new_size <= pool->item_size) {
        /* It already fits where it is. A "shrink" in a fixed-size pool is a no-op, and
         * telling the caller it failed would be a lie. */
        res.err = PROVEN_OK;
        res.value.ptr = (proven_byte_t *)old_ptr;
        res.value.size = new_size;
        return res;
    }

    /* Growing past the item size is the one thing a pool cannot do. It is unsupported,
     * not invalid: the request is perfectly sensible, this allocator just is not the one
     * that can serve it. */
    res.err = PROVEN_ERR_UNSUPPORTED;
    return res;
}

static void proven_pool_free_trait(void *ctx, void *ptr) {
    if (!ctx || !ptr) {
        return;
    }

    proven_pool_t *pool = (proven_pool_t *)ctx;

#if PROVEN_HARDENED || !defined(NDEBUG)
    if (((uintptr_t)ptr) % pool->item_align != 0) {
        proven_panic("proven_pool_free: mismatched item alignment");
        return;
    }

    for (proven_size_t i = 0; i < pool->bin_len; i++) {
        if (pool->bin[i] == ptr) {
            proven_panic("proven_pool_free: double free detected");
            return;
        }
    }
#endif

    if (pool->bin_len < pool->bin_cap) {
#if PROVEN_HARDENED || !defined(NDEBUG)
        proven_sys_mem_set(ptr, 0xDD, pool->item_size);
#endif
        pool->bin[pool->bin_len] = ptr;
        pool->bin_len++;
    } else {
        if (pool->base_alloc.free_fn) {
            pool->base_alloc.free_fn(pool->base_alloc.ctx, ptr);
        }
    }
}

proven_allocator_t proven_pool_as_allocator(proven_pool_t *pool) {
    if (!pool) {
        return (proven_allocator_t){0};
    }
    proven_allocator_t alloc = {0};
    alloc.ctx = pool;
    alloc.alloc_fn = proven_pool_alloc_trait;
    alloc.realloc_fn = proven_pool_realloc_trait;
    alloc.free_fn = proven_pool_free_trait;
    return alloc;
}

void proven_pool_destroy(proven_pool_t *pool) {
    if (!pool) return;
    
    if (pool->base_alloc.free_fn) {
        // Free cached items in the bin
        for (proven_size_t i = 0; i < pool->bin_len; ++i) {
            pool->base_alloc.free_fn(pool->base_alloc.ctx, pool->bin[i]);
        }
        // Free the bin itself
        if (pool->bin) {
            pool->base_alloc.free_fn(pool->base_alloc.ctx, pool->bin);
        }
    }
    
    pool->bin = 0;
    pool->bin_len = 0;
    pool->bin_cap = 0;
    pool->item_size = 0;
    pool->item_align = 0;
    pool->base_alloc = (proven_allocator_t){0};
}
