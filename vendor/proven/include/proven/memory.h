#ifndef PROVEN_MEMORY_H
#define PROVEN_MEMORY_H

#include "types.h"
#include "error.h"

/**
 * @file memory.h
 * @brief Core memory block definitions.
 */

/**
 * @brief Represents an owned memory block.
 */
typedef struct {
    proven_byte_t *ptr;
    proven_size_t  size;
} proven_mem_t;

/**
 * @brief Represents a non-owning read-only view of memory.
 */
typedef struct {
    const proven_byte_t *ptr;
    proven_size_t        size;
} proven_mem_view_t;

/**
 * @brief Represents a non-owning read-write slice of memory.
 */
typedef struct {
    proven_byte_t *ptr;
    proven_size_t  size;
} proven_mem_mut_t;

/**
 * @brief Result wrapper for a proven_mem_mut_t.
 * Replaces out-pointers for error handling.
 */
typedef struct {
    proven_err_t     err;
    proven_mem_mut_t value;
} proven_result_mem_mut_t;

/**
 * @brief Result wrapper for a proven_mem_view_t.
 */
typedef struct {
    proven_err_t      err;
    proven_mem_view_t value;
} proven_result_mem_view_t;

/* --- Creation Helpers --- */

/**
 * @brief Creates a read-only view from an owned memory block.
 */
static inline proven_mem_view_t proven_mem_view_from_owned(proven_mem_t mem) {
    return (proven_mem_view_t){ .ptr = mem.ptr, .size = mem.size };
}

/**
 * @brief Creates a read-write slice from an owned memory block.
 */
static inline proven_mem_mut_t proven_mem_mut_from_owned(proven_mem_t mem) {
    return (proven_mem_mut_t){ .ptr = mem.ptr, .size = mem.size };
}

/* --- Slicing Utilities --- */

/**
 * @brief Slices a view into a sub-view.
 * @note Unsafe. Does no bounds checking.
 */
static inline proven_mem_view_t proven_mem_view_slice_unchecked(proven_mem_view_t view, proven_size_t offset, proven_size_t size) {
    return (proven_mem_view_t){ .ptr = view.ptr + offset, .size = size };
}

/**
 * @brief Slices a view safely with bounds checking.
 */
static inline proven_result_mem_view_t proven_mem_view_slice_checked(proven_mem_view_t view, proven_size_t offset, proven_size_t size) {
    if (view.size > 0 && !view.ptr) {
        return (proven_result_mem_view_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    if (offset > view.size || size > view.size - offset) {
        return (proven_result_mem_view_t){ .err = PROVEN_ERR_OUT_OF_BOUNDS };
    }
    if (size == 0) {
        return (proven_result_mem_view_t){ .value = { .ptr = (const proven_byte_t*)0, .size = 0 }, .err = PROVEN_OK };
    }
    return (proven_result_mem_view_t){ .value = { .ptr = view.ptr + offset, .size = size }, .err = PROVEN_OK };
}

/**
 * @brief Slices a mut-slice into a smaller mut-slice.
 * @note Unsafe. Does no bounds checking.
 */
static inline proven_mem_mut_t proven_mem_mut_slice_unchecked(proven_mem_mut_t mut, proven_size_t offset, proven_size_t size) {
    return (proven_mem_mut_t){ .ptr = mut.ptr + offset, .size = size };
}

/**
 * @brief Slices a mut-slice safely with bounds checking.
 */
static inline proven_result_mem_mut_t proven_mem_mut_slice_checked(proven_mem_mut_t mut, proven_size_t offset, proven_size_t size) {
    if (mut.size > 0 && !mut.ptr) {
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    if (offset > mut.size || size > mut.size - offset) {
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_OUT_OF_BOUNDS };
    }
    if (size == 0) {
        return (proven_result_mem_mut_t){ .value = { .ptr = (proven_byte_t*)0, .size = 0 }, .err = PROVEN_OK };
    }
    return (proven_result_mem_mut_t){ .value = { .ptr = mut.ptr + offset, .size = size }, .err = PROVEN_OK };
}

/**
 * @brief Safely checks if a pointer and size fall entirely within a base allocation range.
 * This avoids UB with < or >= comparisons on unrelated pointers.
 * 
 * @param base The start of the allocated memory block.
 * @param cap The total capacity of the allocated memory block.
 * @param ptr The pointer to check.
 * @param size The size of the memory region to check.
 * @param out_offset Optional. If inside, stores the byte offset from base to ptr.
 * @return true if [ptr, ptr+size) lies within [base, base+cap].
 *         For size == 0, ptr == base + cap is accepted as a valid empty range.
 */
static inline _Bool proven_range_contains_ptr(const void *base, proven_size_t cap, const void *ptr, proven_size_t size, proven_size_t *out_offset) {
    if (!base || !ptr) return false;

    proven_uintptr_t b = (proven_uintptr_t)base;
    proven_uintptr_t p = (proven_uintptr_t)ptr;

    proven_uintptr_t end;
    if (PROVEN_CKD_ADD(&end, b, cap)) return false;

    if (p < b || p > end) return false;

    proven_uintptr_t p_end;
    if (PROVEN_CKD_ADD(&p_end, p, size)) return false;
    if (p_end > end) return false;

    if (out_offset) {
        *out_offset = (proven_size_t)(p - b);
    }

    return true;
}

/**
 * @brief Compares two memory regions safely.
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2.
 */
int proven_memcmp(const void *s1, const void *s2, proven_size_t size);

/**
 * @brief Bounded copy of a read-only byte view into a destination buffer.
 *
 * Copies src.size bytes into dst, whose capacity is dst_cap. Returns
 * PROVEN_ERR_OUT_OF_BOUNDS if the source would not fit, PROVEN_ERR_INVALID_ARG
 * on a null pointer with non-zero size, and PROVEN_OK otherwise (a zero-size
 * source is a no-op). The regions must not overlap.
 */
proven_err_t proven_mem_copy(void *dst, proven_size_t dst_cap, proven_mem_view_t src);

/**
 * @brief Bounded, overlap-safe move of a read-only byte view into a buffer.
 *
 * Same contract as proven_mem_copy (OUT_OF_BOUNDS if it would overflow,
 * INVALID_ARG on a null pointer with non-zero size, no-op for a zero-size
 * source), but the source and destination regions may overlap.
 */
proven_err_t proven_mem_move(void *dst, proven_size_t dst_cap, proven_mem_view_t src);

#endif /* PROVEN_MEMORY_H */
