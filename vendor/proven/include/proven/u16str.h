#ifndef PROVEN_U16STR_H
#define PROVEN_U16STR_H

#ifndef PROVEN_NO_U16STR

#include "proven/types.h"
#include "proven/buffer.h"
#include "proven/allocator.h"

/**
 * @file u16str.h
 * @brief String wrappers enforcing Unsigned 16-bit (encoding-agnostic) logic.
 * 
 * Length is calculated in units of u16, and the null terminator is a u16 0.
 * Encoding assumptions (like UTF-16) are deferred to higher layers.
 */

/**
 * @brief Zero-copy view into a U16 string.
 */
typedef struct {
    const proven_u16 *ptr;
    proven_size_t     size; /**< Number of u16 units, not bytes */
} proven_u16str_view_t;

/**
 * @brief Managed U16 string structure.
 *
 * @warning **The string does not remember its allocator, and nothing checks that you pass the
 *          same one.** create, reserve, append_grow and destroy must all be given the
 *          allocator the string was created with. Passing another - destroying an arena string
 *          through the heap, growing a heap string through an arena - is not an error this
 *          library can report: it corrupts the allocator's state, and the damage surfaces
 *          later, somewhere else. Keep a string and its allocator together in your own code.
 *          (Storing the allocator in every string was rejected - it doubles the struct and has
 *          no meaning for a borrowed string; see B-023.)
 */
typedef struct {
    proven_buf_t internal; /**< Capacity and length are tracked in bytes */
} proven_u16str_t;

/**
 * @brief Result wrapper for u16 string creation.
 */
typedef struct {
    proven_err_t    err;
    proven_u16str_t value;
} proven_result_u16str_t;

/**
 * @brief A borrowed u16 view, or the error that stopped it (the u16 line readers return this).
 */
typedef struct {
    proven_err_t         err;
    proven_u16str_view_t val;
} proven_result_u16str_view_t;

#define PROVEN_U16_LIT(s) ((proven_u16str_view_t){ u##s, (sizeof(u##s) / sizeof((u##s)[0])) - 1 })

[[nodiscard]] proven_result_u16str_t proven_u16str_create(proven_allocator_t alloc, proven_size_t unit_limit);
[[nodiscard]] proven_result_u16str_t proven_u16str_create_from_view(proven_allocator_t alloc, proven_u16str_view_t view);
/** @brief Free an owned string. `alloc` MUST be the allocator it was created with - unchecked. */
void proven_u16str_destroy(proven_allocator_t alloc, proven_u16str_t *str);

/**
 * @brief Appends data to a U16 string. 
 * CATEGORY: Atomic Fixed-Capacity
 * 
 * If the data fits entirely within the current capacity, it appends and returns PROVEN_OK.
 * If not, it returns PROVEN_ERR_OUT_OF_BOUNDS without modifying the original string.
 */
[[nodiscard]] proven_err_t proven_u16str_append(proven_u16str_t *str, proven_u16str_view_t data);

/**
 * @brief Appends data as much as possible.
 * CATEGORY: Best-Effort/Truncating
 */
[[nodiscard]] proven_result_size_t proven_u16str_append_partial(proven_u16str_t *str, proven_u16str_view_t data);

/**
 * @brief Appends a view. Potentially grows the buffer using the provided allocator.
 * CATEGORY: Atomic Growable
 */
[[nodiscard]] proven_err_t proven_u16str_append_grow(proven_allocator_t alloc, proven_u16str_t *str, proven_u16str_view_t data);

[[nodiscard]]
static inline const proven_u16* proven_u16str_as_ptr(const proven_u16str_t *str) {
    return (const proven_u16*)str->internal.ptr;
}

[[nodiscard]]
static inline proven_size_t proven_u16str_len(const proven_u16str_t *str) {
    return str->internal.len / sizeof(proven_u16);
}

#endif /* PROVEN_NO_U16STR */

#endif // PROVEN_U16STR_H
