#ifndef PROVEN_ARRAY_H
#define PROVEN_ARRAY_H

#include <stdalign.h>
#include "proven/types.h"
#include "proven/error.h"
#include "proven/allocator.h"
#include "proven/align.h"

/**
 * @file array.h
 * @brief Dynamic array implementation with bounds checks and contiguous memory locality.
 */

typedef struct {
    proven_allocator_t alloc;    // Allocator trait driving memory decisions
    proven_byte_t *data;         // Backing memory slice representing physical capacity space
    proven_size_t len;           // The current active element count
    proven_size_t cap;           // The capacity in elements
    proven_size_t elem_size;     // Size of a single element (e.g., sizeof(int))
    proven_size_t align;         // The alignment boundary mandated by the stored type
} proven_array_t;

typedef struct {
    proven_err_t err;
    proven_array_t value;
} proven_result_array_t;

// -------------------------------------------------------------
// Type-Agnostic Core C API
// -------------------------------------------------------------

[[nodiscard]] proven_result_array_t proven_array_create(proven_allocator_t alloc, proven_size_t init_cap, proven_size_t elem_size, proven_size_t align);

/**
 * @brief Validates the structural integrity of the public array fields.
 */
[[nodiscard]] bool proven_array_is_valid(const proven_array_t *arr);

/**
 * @brief Pre-allocates memory for the array to reach at least `new_cap` capacity.
 * Useful when working with arena allocators to prevent dead storage from reallocations.
 */
[[nodiscard]] proven_err_t proven_array_reserve(proven_array_t *arr, proven_size_t new_cap);

[[nodiscard]] proven_err_t proven_array_push(proven_array_t *arr, const void *element);

[[nodiscard]] proven_err_t proven_array_pop(proven_array_t *arr, void *out_element);

/**
 * @brief A pointer into the container's storage. It dies the next time the container grows.
 *
 * @warning The returned pointer is INVALIDATED by any operation that may reallocate -
 *          push, reserve, set, append, an insert that triggers a rehash. Using it
 *          afterwards is a use-after-free, and the sanitizers will say so. Hold the index
 *          or the key, not the pointer, across a mutation.
 */
[[nodiscard]] void* proven_array_get_mut(proven_array_t *arr, proven_size_t index);
[[nodiscard]] const void* proven_array_get(const proven_array_t *arr, proven_size_t index);

void proven_array_destroy(proven_array_t *arr);

// -------------------------------------------------------------
// Type-Safe Strict Macro Wrappers
// -------------------------------------------------------------

/**
 * @brief Initializes a generic proven array with type-safe size and alignment deduction.
 */
#define PROVEN_ARRAY_INIT(alloc, type, init_cap) \
    proven_array_create((alloc), (init_cap), sizeof(type), alignof(type))

/**
 * @brief Pushes an element into the dynamic array, creating a temporary block to securely capture rvalues.
 */
#define PROVEN_ARRAY_PUSH(arr_ptr, type, value) \
    proven_array_push((arr_ptr), (type[]){(value)})

/**
 * @brief Pops the last element. Can pass NULL for out_ptr to just discard.
 */
#define PROVEN_ARRAY_POP(arr_ptr, type, out_ptr) \
    proven_array_pop((arr_ptr), (out_ptr))

/**
 * @brief Retrieves a typed pointer to an element in the array.
 */
#define PROVEN_ARRAY_GET(arr_ptr, type, index) \
    ((const type*)proven_array_get((arr_ptr), (index)))

/**
 * @brief Retrieves a typed mutable pointer to an element in the array.
 */
#define PROVEN_ARRAY_GET_MUT(arr_ptr, type, index) \
    ((type*)proven_array_get_mut((arr_ptr), (index)))

/**
 * @brief Cleans up internal structures delegating array data memory returns back through the contextual VTable alloc.
 */
#define PROVEN_ARRAY_DESTROY(arr_ptr) \
    proven_array_destroy(arr_ptr)

#endif /* PROVEN_ARRAY_H */
