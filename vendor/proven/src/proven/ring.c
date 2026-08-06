#include "proven/ring.h"
#include "../../platform/proven_sys_mem.h"

proven_result_ring_t proven_ring_create(proven_allocator_t alloc, proven_size_t cap, proven_size_t elem_size, proven_size_t align) {
    proven_result_ring_t res = {0};
    
    if (cap == 0 || elem_size == 0 || !proven_alloc_is_valid(alloc) || !proven_is_pow2(align)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_size_t bytes_needed;
    if (PROVEN_CKD_MUL(&bytes_needed, cap, elem_size)) {
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
        return res;
    }
    proven_result_mem_mut_t alloc_res = alloc.alloc_fn(alloc.ctx, bytes_needed, align);
    
    if (!PROVEN_IS_OK(alloc_res.err)) {
        res.err = alloc_res.err;
        return res;
    }

    proven_ring_t ring = {
        .alloc = alloc,
        .internal = alloc_res.value,
        .head = 0,
        .tail = 0,
        .len = 0,
        .cap = cap,
        .elem_size = elem_size,
        .align = align
    };

    res.err = PROVEN_OK;
    res.value = ring;
    return res;
}

bool proven_ring_is_valid(const proven_ring_t *ring) {
    if (!ring) return false;
    if (ring->elem_size == 0 || !proven_is_pow2(ring->align)) return false;
    if (ring->cap == 0) return false;
    if (ring->len > ring->cap) return false;
    if (ring->head >= ring->cap || ring->tail >= ring->cap) return false;
    if ((ring->head + ring->len) % ring->cap != ring->tail) return false;
    if (!ring->internal.ptr) return false;
    proven_size_t bytes_needed;
    if (PROVEN_CKD_MUL(&bytes_needed, ring->cap, ring->elem_size)) return false;
    if (ring->internal.size < bytes_needed) return false;
    if (!proven_alloc_is_valid(ring->alloc)) return false;
    return true;
}

proven_err_t proven_ring_push(proven_ring_t *ring, const void *element) {
    if (!ring || !element) return PROVEN_ERR_INVALID_ARG;
    
    // Fixed-capacity ring; push fails when the buffer is full avoiding re-allocations
    if (ring->len >= ring->cap) return PROVEN_ERR_OUT_OF_BOUNDS;

    proven_size_t dst_offset;
    proven_size_t required_bytes;
    if (PROVEN_CKD_MUL(&dst_offset, ring->tail, ring->elem_size) || 
        PROVEN_CKD_ADD(&required_bytes, dst_offset, ring->elem_size) ||
        ring->internal.size < required_bytes) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    proven_byte_t *dst = ring->internal.ptr + dst_offset;
    proven_sys_mem_copy(dst, element, ring->elem_size);

    // Wrap around to the beginning of the ring.
    ring->tail = (ring->tail + 1) % ring->cap;
    ring->len++;
    
    return PROVEN_OK;
}

proven_err_t proven_ring_pop(proven_ring_t *ring, void *out_element) {
    if (!ring || ring->len == 0) return PROVEN_ERR_OUT_OF_BOUNDS; // Fail if empty

    if (out_element) {
        proven_size_t src_offset;
        proven_size_t required_bytes;
        if (PROVEN_CKD_MUL(&src_offset, ring->head, ring->elem_size) || 
            PROVEN_CKD_ADD(&required_bytes, src_offset, ring->elem_size) ||
            ring->internal.size < required_bytes) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        const proven_byte_t *src = ring->internal.ptr + src_offset;
        proven_sys_mem_copy(out_element, src, ring->elem_size);
    }

    // Wrap around to the beginning of the ring.
    ring->head = (ring->head + 1) % ring->cap;
    ring->len--;
    
    return PROVEN_OK;
}

void proven_ring_destroy(proven_ring_t *ring) {
    if (!ring) return;
    if (ring->internal.ptr) {
        ring->alloc.free_fn(ring->alloc.ctx, ring->internal.ptr);
    }
    ring->internal.ptr = (void*)0;
    ring->internal.size = 0;
    ring->head = 0;
    ring->tail = 0;
    ring->len = 0;
    ring->cap = 0;
}
