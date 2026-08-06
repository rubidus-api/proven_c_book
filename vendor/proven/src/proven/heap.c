#include "proven/heap.h"

#ifdef PROVEN_FREESTANDING

proven_allocator_t proven_heap_allocator(void) {
    return (proven_allocator_t){0};
}

#else

#include "../../platform/proven_sys_mem.h" // Platform Abstraction Layer Injection

static proven_result_mem_mut_t proven_heap_alloc_trait(void *ctx, proven_size_t size, proven_size_t align) {
    (void)ctx;
    proven_result_mem_mut_t res = {0};

    /* A zero-byte request is a caller bug, and it is NOT "out of memory" - nothing was
     * out of anything. The arena answered the same call with PROVEN_OK and a live pointer
     * to nothing, so trait-generic code could not be written against either answer. */
    if (size == 0) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    // Defer entirely to the OS Platform Abstraction Layer
    void *ptr = proven_sys_mem_alloc(size, align);

    if (!ptr) {
        res.err = PROVEN_ERR_NOMEM;
        return res;
    }
    
    res.err = PROVEN_OK;
    res.value.ptr = (proven_byte_t *)ptr;
    res.value.size = size;
    return res;
}

static proven_result_mem_mut_t proven_heap_realloc_trait(void *ctx, void *old_ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align) {
    (void)ctx;
    proven_result_mem_mut_t res = {0};

    void *ptr = proven_sys_mem_realloc(old_ptr, old_size, new_size, align);
    if (!ptr && new_size > 0) {
        res.err = PROVEN_ERR_NOMEM;
        return res;
    }

    res.err = PROVEN_OK;
    res.value.ptr = (proven_byte_t*)ptr;
    res.value.size = new_size;
    return res;
}

static void proven_heap_free_trait(void *ctx, void *ptr) {
    (void)ctx;
    proven_sys_mem_free(ptr);
}

proven_allocator_t proven_heap_allocator(void) {
    return (proven_allocator_t){ 
        .ctx = 0, 
        .alloc_fn = proven_heap_alloc_trait,
        .realloc_fn = proven_heap_realloc_trait,
        .free_fn = proven_heap_free_trait
    };
}

#endif /* PROVEN_FREESTANDING */
