#include "proven/buffer.h"
#include "proven/align.h"
#include "../../platform/proven_sys_mem.h"

proven_result_buf_t proven_buf_create(proven_allocator_t alloc, proven_size_t cap) {
    proven_result_buf_t res = {0};
    if (!proven_alloc_is_valid(alloc) || cap == 0) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    
    // Allocate the underlying capacity utilizing the allocator trait injected
    proven_result_mem_mut_t alloc_res = alloc.alloc_fn(alloc.ctx, cap, PROVEN_DEFAULT_ALIGNMENT);
    if (!proven_is_ok(alloc_res.err)) {
        res.err = alloc_res.err;
        return res;
    }
    
    res.err = PROVEN_OK;
    res.value.ptr = alloc_res.value.ptr;
    res.value.len = 0;
    res.value.cap = cap;
    
    return res;
}

proven_err_t proven_buf_append(proven_buf_t *buf, proven_mem_view_t data) {
    if (!buf || (data.size > 0 && !data.ptr)) {
        return PROVEN_ERR_INVALID_ARG;
    }
    proven_size_t new_len;
    if (PROVEN_CKD_ADD(&new_len, buf->len, data.size) || new_len > buf->cap) {
        return PROVEN_ERR_OUT_OF_BOUNDS; 
    }
    if (!buf->ptr) return PROVEN_ERR_INVALID_ARG;
    
    // Move data into the buffer so overlapping source views remain well-defined.
    proven_sys_mem_move(buf->ptr + buf->len, data.ptr, data.size);
    
    buf->len = new_len;
    return PROVEN_OK;
}

void proven_buf_destroy(proven_allocator_t alloc, proven_buf_t *buf) {
    if (!buf || !buf->ptr) return;
    if (alloc.free_fn) alloc.free_fn(alloc.ctx, buf->ptr);
    buf->ptr = (void*)0;
    buf->len = 0;
    buf->cap = 0;
}
