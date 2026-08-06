#ifndef PROVEN_U8STR_H
#define PROVEN_U8STR_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/buffer.h"
#include "proven/allocator.h"

/**
 * @file u8str.h
 * @brief String wrappers enforcing Unsigned 8-bit (encoding-agnostic) logic and null-termination guarantees.
 * v26.05.08n
 *
 * Length is calculated in units of u8. Encoding assumptions (like UTF-8) are deferred to higher layers.
 */

/**
 * @brief Represents a read-only u8-string view. 
 */
typedef struct {
    const proven_byte_t *ptr;
    proven_size_t        size;
} proven_u8str_view_t;

/**
 * @brief Represents a mutable u8-string view.
 */
typedef struct {
    proven_byte_t *ptr;
    proven_size_t  size;
} proven_u8str_mut_t;

/**
 * @brief A string buffer tracking U8 bytes.
 *
 * `borrowed` is false (0) for allocator-owned strings created by
 * proven_u8str_create / _create_from_view, so a zero-initialized handle is
 * owned by default. It is set true only by proven_u8str_borrow, which wraps
 * caller-owned memory: for a borrowed string the growing operations refuse to
 * reallocate and proven_u8str_destroy is a no-op.
 */
typedef struct {
    proven_buf_t internal;
    bool         borrowed;
} proven_u8str_t;

/**
 * @brief Result wrapper for a string creation.
 */
typedef struct {
    proven_err_t err;
    proven_u8str_t value;
} proven_result_u8str_t;

/**
 * @brief Result wrapper for explicitly allocating a safe C-String.
 */
typedef struct {
    proven_err_t err;
    const char *value;
} proven_result_cstr_t;

/**
 * @brief Macro to create a u8str view strictly from a C string literal at compile time.
 */
#define PROVEN_LIT(s) ((proven_u8str_view_t){ .ptr = (const proven_byte_t *)("" s), .size = sizeof("" s) - 1 })
#define PROVEN_LIT_INIT(s) { .ptr = (const proven_byte_t *)(s), .size = sizeof(s) - 1 }

#include "proven/align.h"

#define PROVEN_INDEX_NOT_FOUND ((proven_size_t)-1)

[[nodiscard]] proven_result_u8str_t proven_u8str_create(proven_allocator_t alloc, proven_size_t limit);
[[nodiscard]] proven_result_u8str_t proven_u8str_create_from_view(proven_allocator_t alloc, proven_u8str_view_t view);

/**
 * @brief Wraps caller-owned memory as a fixed-capacity string (no allocation).
 *
 * The returned string borrows `[buf, buf+cap)`; `cap` is the total byte
 * capacity *including* the NUL terminator, so it can hold `cap - 1` content
 * bytes. It starts empty and NUL-terminated. No ownership is taken: the caller
 * keeps `buf` alive for the string's lifetime, and proven_u8str_destroy on it
 * is a no-op.
 *
 * The fixed-capacity operations (append, append_partial, replace_at, insert,
 * remove, append_fmt, append_fmt_trunc) work as usual. The growing operations
 * (reserve, *_grow, append_byte, append_fmt_grow) still succeed while the data
 * fits within `cap`, but return PROVEN_ERR_OUT_OF_BOUNDS instead of
 * reallocating caller memory once it would not.
 *
 * `cap == 0` or `buf == NULL` yields an empty (cap 0) borrowed string.
 */
[[nodiscard]] proven_u8str_t proven_u8str_borrow(proven_byte_t *buf, proven_size_t cap);

/**
 * @brief Truncates a string to empty, keeping its buffer and capacity.
 *
 * Lets an owned or borrowed string be reused (e.g. rebuilt each frame) without
 * reallocating. Returns PROVEN_ERR_INVALID_ARG for a null or capacity-0 string.
 */
[[nodiscard]] proven_err_t proven_u8str_reset(proven_u8str_t *str);

/**
 * @brief Validates the structural integrity of the public string fields.
 */
[[nodiscard]] bool proven_u8str_is_valid(const proven_u8str_t *str);

/**
 * @brief Pre-allocates memory for the string to reach at least `new_cap` capacity.
 * Useful when working with arena allocators to prevent dead storage from reallocations.
 */
[[nodiscard]] proven_err_t proven_u8str_reserve(proven_allocator_t alloc, proven_u8str_t *str, proven_size_t new_cap);

/**
 * @brief Appends data to a string. 
 * CATEGORY: Atomic Fixed-Capacity
 * 
 * If the data fits entirely within the current capacity, it appends and returns PROVEN_OK.
 * If not, it returns PROVEN_ERR_OUT_OF_BOUNDS without modifying the original string.
 * This is guaranteed by performing a capacity check before any writes.
 */
[[nodiscard]] proven_err_t proven_u8str_append(proven_u8str_t *str, proven_u8str_view_t data);

/**
 * @brief Appends data to a string as much as possible.
 * CATEGORY: Best-Effort/Truncating
 * 
 * Partial modification is allowed. Always ensures valid null-termination.
 * If the full data cannot be appended, it returns PROVEN_ERR_OUT_OF_BOUNDS but 
 * populates the result with the actual number of bytes written.
 */
[[nodiscard]] proven_result_size_t proven_u8str_append_partial(proven_u8str_t *str, proven_u8str_view_t data);

/**
 * @brief Appends data to a string, growing the buffer if necessary.
 * CATEGORY: Atomic Growable
 * 
 * If reallocation fails, returns PROVEN_ERR_NOMEM and leaves the string unchanged.
 * Does NOT fallback to partial append on growth failure.
 */
[[nodiscard]] proven_err_t proven_u8str_append_grow(proven_allocator_t alloc, proven_u8str_t *str, proven_u8str_view_t data);

[[nodiscard]] proven_err_t proven_u8str_append_byte(proven_allocator_t alloc, proven_u8str_t *str, proven_u8 b);

[[nodiscard]] proven_err_t proven_u8str_replace_at(proven_u8str_t *str, proven_size_t index, proven_size_t old_len, proven_u8str_view_t data);
[[nodiscard]] proven_err_t proven_u8str_insert(proven_u8str_t *str, proven_size_t index, proven_u8str_view_t data);
[[nodiscard]] proven_err_t proven_u8str_remove(proven_u8str_t *str, proven_size_t index, proven_size_t len);

/**
 * @brief Growing variants of replace_at / insert.
 * CATEGORY: Atomic Growable
 *
 * Same semantics as proven_u8str_replace_at / proven_u8str_insert, but the
 * buffer is grown (doubling capacity) when the edit does not fit, instead of
 * returning PROVEN_ERR_OUT_OF_BOUNDS. On allocation failure the string is left
 * unchanged and the allocator error is returned. `index` must be <= length.
 * `data` must not alias the string buffer when the edit shifts the tail.
 */
[[nodiscard]] proven_err_t proven_u8str_replace_at_grow(proven_allocator_t alloc, proven_u8str_t *str, proven_size_t index, proven_size_t old_len, proven_u8str_view_t data);
[[nodiscard]] proven_err_t proven_u8str_insert_grow(proven_allocator_t alloc, proven_u8str_t *str, proven_size_t index, proven_u8str_view_t data);
/**
 * @brief Replaces the first occurrence of a target substring with a replacement.
 *
 * If target is not found, the string is left unchanged and PROVEN_OK is returned.
 * Use proven_u8str_view_find() first if the caller needs to distinguish
 * "not found" from "replaced".
 */
[[nodiscard]] proven_err_t proven_u8str_replace_first(proven_u8str_t *str, proven_size_t start_offset, proven_u8str_view_t target, proven_u8str_view_t replacement);

[[nodiscard]] proven_size_t proven_u8str_view_find(proven_u8str_view_t haystack, proven_size_t start_offset, proven_u8str_view_t needle);
[[nodiscard]] int proven_u8str_view_starts_with(proven_u8str_view_t str, proven_u8str_view_t prefix);
[[nodiscard]] int proven_u8str_view_ends_with(proven_u8str_view_t str, proven_u8str_view_t suffix);
[[nodiscard]] proven_u8str_view_t proven_u8str_view_slice(proven_u8str_view_t str, proven_size_t index, proven_size_t len);

/**
 * @brief Zero-cost extraction of a standard C string pointer inherently guaranteed by internal structure.
 */
static inline const char* proven_u8str_as_cstr(const proven_u8str_t *str) {
    return (const char*)str->internal.ptr;
}

/**
 * @brief Converts a read-only u8-string view to a generic read-only memory view.
 */
static inline proven_mem_view_t proven_mem_view_from_u8(proven_u8str_view_t view) {
    return (proven_mem_view_t){ .ptr = view.ptr, .size = view.size };
}

/**
 * @brief Allocates and creates a null-terminated C-string from an arbitrary u8str view slicing block.
 * Requires an explicit allocator (Arena or Heap).
 */
[[nodiscard]] proven_result_cstr_t proven_u8str_view_to_cstr(proven_u8str_view_t view, proven_allocator_t alloc);

[[nodiscard]] proven_size_t proven_cstr_len(const char *s);

[[nodiscard]] static inline proven_u8str_view_t proven_u8str_view_from_cstr(const char *s) {
    if (!s) return (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 };
    return (proven_u8str_view_t){ .ptr = (const proven_byte_t *)s, .size = proven_cstr_len(s) };
}

[[nodiscard]] int proven_u8str_view_eq(proven_u8str_view_t a, proven_u8str_view_t b);

void proven_u8str_destroy(proven_allocator_t alloc, proven_u8str_t *str);

/**
 * @brief Zero-cost downgrade of a mutable string to a read-only view.
 *
 * @warning The view points into the string's own storage, so it is INVALIDATED by any
 *          operation that may reallocate: append_grow, reserve, a growing fmt. Take the
 *          view again after a mutation rather than holding one across it.
 */
static inline proven_u8str_view_t proven_u8str_as_view(const proven_u8str_t *str) {
    if (!str) return (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 };
    return (proven_u8str_view_t){ .ptr = str->internal.ptr, .size = str->internal.len };
}

#endif /* PROVEN_U8STR_H */
