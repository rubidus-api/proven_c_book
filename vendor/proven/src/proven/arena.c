#include "proven/arena.h"

proven_result_mem_mut_t proven_arena_alloc_aligned(proven_arena_t *arena, proven_size_t size, proven_size_t align) {
    proven_result_mem_mut_t res = {0};

    if (!arena || !arena->backing.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (!proven_is_pow2(align)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (size == 0) {
        /* Same rule as the heap: a zero-byte allocation is a caller bug. This used to
         * return PROVEN_OK with a live pointer to nothing. */
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (arena->offset > arena->backing.size) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    // Calculate the current unaligned address base for pointer arithmetic
    proven_uintptr_t base_addr = (proven_uintptr_t)arena->backing.ptr;
    proven_uintptr_t current_addr;
    if (PROVEN_CKD_ADD(&current_addr, base_addr, (proven_uintptr_t)arena->offset)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    
    // Apply zero-overhead alignment math
    proven_uintptr_t aligned_addr = proven_uintptr_align_up(current_addr, align);
    if (aligned_addr < base_addr) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    proven_size_t padding = (proven_size_t)(aligned_addr - current_addr);

    // Strict bounds check preventing integer overflow
    proven_size_t required;
    if (PROVEN_CKD_ADD(&required, arena->offset, (proven_size_t)padding)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    if (PROVEN_CKD_ADD(&required, required, size)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    if (required > arena->backing.size) {
        res.err = PROVEN_ERR_NOMEM;
        return res;
    }

    // Commit the allocation
    res.err = PROVEN_OK;
    res.value.ptr = arena->backing.ptr + required - size;
    res.value.size = size;

    // Advance the offset state
    arena->offset = required;

    return res;
}

proven_result_mem_mut_t proven_arena_realloc_aligned(proven_arena_t *arena, void *old_ptr, proven_size_t old_size, proven_size_t new_size, proven_size_t align) {
    proven_result_mem_mut_t res = {0};

    if (!arena || !arena->backing.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (!proven_is_pow2(align)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (new_size == 0) {
        /* The trait says: free the block, return a null pointer, PROVEN_OK. The arena
         * frees nothing (that is what an arena is), but it must still answer the way the
         * trait says it will - it used to hand back a NON-null pointer, so trait-generic
         * code that tests `ptr == NULL` to learn the block is gone behaved differently
         * depending on which allocator it had been given. */
        res.err = PROVEN_OK;
        res.value.ptr = NULL;
        res.value.size = 0;
        return res;
    }

    if (!old_ptr) {
        return proven_arena_alloc_aligned(arena, new_size, align);
    }

    proven_uintptr_t old_addr = (proven_uintptr_t)old_ptr;
    proven_uintptr_t base_addr = (proven_uintptr_t)arena->backing.ptr;

    proven_uintptr_t raw_offset;
    if (PROVEN_CKD_SUB(&raw_offset, old_addr, base_addr)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (raw_offset > (proven_uintptr_t)arena->offset) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_size_t old_offset = (proven_size_t)raw_offset;
    proven_size_t old_offset_end;
    if (PROVEN_CKD_ADD(&old_offset_end, old_offset, old_size)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (old_offset_end > arena->offset) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    bool is_tail = (old_offset_end == arena->offset);

    // If old_ptr is the most recent arena allocation, resize it in place.
    if (is_tail) {
        // Yes! ZERO-COPY in-place extension.
        proven_size_t new_offset;
        // Re-calculations bounding the diff gracefully handling mathematical shrinkage securely!
        if (new_size >= old_size) {
            proven_size_t diff = new_size - old_size;
            if (PROVEN_CKD_ADD(&new_offset, arena->offset, diff)) {
                 res.err = PROVEN_ERR_OVERFLOW;
                 return res;
            }
        } else {
            proven_size_t diff = old_size - new_size;
            if (diff > arena->offset) {
                res.err = PROVEN_ERR_NOMEM;
                return res;
            }
            new_offset = arena->offset - diff;
        }
        
        if (new_offset > arena->backing.size) {
            res.err = PROVEN_ERR_NOMEM;
            return res;
        }

        // Extends or Shrinks physically modifying internal tracker indexes rapidly
        arena->offset = new_offset;
        
        // Recover original pointer efficiently preserving correct array-bounds provenance contexts.
        res.err = PROVEN_OK;
        res.value.ptr = arena->backing.ptr + old_offset;
        res.value.size = new_size;
        return res;
    }

    if (new_size <= old_size) {
        /* A pure SHRINK of a non-tail block needs no new memory at all - the block just
         * becomes smaller where it stands. Routing it through a fresh tail allocation made
         * it fail with PROVEN_ERR_NOMEM on a full arena, which is an absurd answer to
         * "please use less". */
        res.err = PROVEN_OK;
        res.value.ptr = arena->backing.ptr + old_offset;
        res.value.size = new_size;
        return res;
    }

    // Non-tail reallocation: allocate new block and copy data
    proven_result_mem_mut_t new_alloc = proven_arena_alloc_aligned(arena, new_size, align);
    if (PROVEN_IS_OK(new_alloc.err)) {
        proven_byte_t *src_ptr = arena->backing.ptr + old_offset;
        proven_byte_t *dst = new_alloc.value.ptr;
        proven_size_t copy_amount = old_size < new_size ? old_size : new_size;
        for (proven_size_t i = 0; i < copy_amount; ++i) dst[i] = src_ptr[i];
    }
    
    return new_alloc;
}
