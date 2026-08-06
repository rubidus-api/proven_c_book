#include "proven/memory.h"
#include "../../platform/proven_sys_mem.h"

/**
 * @file memory.c
 * @brief Implementation of memory-related non-inline functions.
 * Currently, core slicing is inlined in headers for performance,
 * but this file serves as the library's memory core object.
 */

int proven_memcmp(const void *s1, const void *s2, proven_size_t size) {
    return proven_sys_mem_cmp(s1, s2, size); // forward to sys
}

proven_err_t proven_mem_copy(void *dst, proven_size_t dst_cap, proven_mem_view_t src) {
    if (src.size == 0) return PROVEN_OK;
    if (!dst || !src.ptr) return PROVEN_ERR_INVALID_ARG;
    if (src.size > dst_cap) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_sys_mem_copy(dst, src.ptr, src.size); // non-overlapping bounded copy
    return PROVEN_OK;
}

proven_err_t proven_mem_move(void *dst, proven_size_t dst_cap, proven_mem_view_t src) {
    if (src.size == 0) return PROVEN_OK;
    if (!dst || !src.ptr) return PROVEN_ERR_INVALID_ARG;
    if (src.size > dst_cap) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_sys_mem_move(dst, src.ptr, src.size); // overlap-safe bounded move
    return PROVEN_OK;
}
