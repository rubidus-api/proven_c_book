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

bool proven_mem_equal_ct(proven_mem_view_t a, proven_mem_view_t b) {
    if (a.size != b.size) return false;
    if (a.size == 0) return true;
    if (!a.ptr || !b.ptr) return false;
    /* Every byte is read and folded in; nothing in the loop depends on what was found. The
     * accumulator is volatile so that a compiler cannot turn the loop into an early exit. */
    volatile proven_byte_t diff = 0;
    for (proven_size_t i = 0; i < a.size; ++i) diff = (proven_byte_t)(diff | (a.ptr[i] ^ b.ptr[i]));
    return diff == 0;
}

void proven_mem_wipe(proven_mem_mut_t mem) {
    if (!mem.ptr) return;
#if defined(__GNUC__) || defined(__clang__)
    /* A fill as fast as any, and then a statement the compiler cannot see into: it must
     * assume the empty assembly reads what the pointer points at, so the fill before it is
     * not a dead store, whatever becomes of the memory afterwards. (A byte at a time through
     * a volatile pointer, as below, cost four processor cycles a byte on a TLS connection's
     * buffers: more than the cipher's share of a record.) */
    proven_sys_mem_zero(mem.ptr, mem.size);
    __asm__ __volatile__("" : : "r"(mem.ptr) : "memory");
#else
    /* Writes through a volatile pointer are observable behaviour and must be performed. */
    volatile proven_byte_t *p = mem.ptr;
    for (proven_size_t i = 0; i < mem.size; ++i) p[i] = 0;
#endif
}
