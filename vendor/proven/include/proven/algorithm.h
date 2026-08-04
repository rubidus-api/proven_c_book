#ifndef PROVEN_ALGORITHM_H
#define PROVEN_ALGORITHM_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/array.h"

/**
 * @file algorithm.h
 * @brief Generic sorting and searching algorithms for proven collections.
 */

/**
 * @brief Comparison function signature.
 * Should return:
 *  - negative if a < b
 *  - zero if a == b
 *  - positive if a > b
 */
typedef int (*proven_compare_fn_t)(const void *a, const void *b);

// -------------------------------------------------------------
// Array Algorithms
// -------------------------------------------------------------

/**
 * @brief Sorts a proven_array_t in place.
 *
 * An introsort: a Bentley-McIlroy three-way partition, an insertion-sort cutoff
 * for small ranges, and a heapsort fallback once the recursion exceeds
 * 2*log2(n) levels.
 *
 * @note O(n log n) is a guarantee, not a typical case. The heapsort fallback is
 *       what makes it one - median-of-three alone can still be driven quadratic
 *       by an adversarial ordering, and a reachable worst case is a denial of
 *       service in any program that sorts data it did not author.
 * @note Duplicate keys are the fast case. Elements equal to the pivot are
 *       collected into a run that is final and never recursed into, so
 *       all-equal input costs a single pass. This matters because
 *       low-cardinality keys - a status column, an enum, a bucket id - are what
 *       callers actually sort by.
 * @note Not stable: equal elements may be reordered.
 * @note `cmp` must be a consistent ordering. An inconsistent comparator yields
 *       an unspecified order (it cannot corrupt memory, but it is still a bug).
 *
 * @param arr Pointer to the array.
 * @param cmp Comparison function.
 */
void proven_array_sort(proven_array_t *arr, proven_compare_fn_t cmp);

/**
 * @brief Performs a binary search on a sorted proven_array_t.
 * @param arr Pointer to the sorted array.
 * @param key Pointer to the element to search for.
 * @param cmp Comparison function.
 * @return Pointer to the found element in the array, or NULL if not found.
 */
void* proven_array_binary_search(const proven_array_t *arr, const void *key, proven_compare_fn_t cmp);

/**
 * @brief Performs a linear search on a proven_array_t.
 * @param arr Pointer to the array.
 * @param key Pointer to the element to search for.
 * @param cmp Comparison function.
 * @return Pointer to the found element in the array, or NULL if not found.
 */
void* proven_array_linear_search(const proven_array_t *arr, const void *key, proven_compare_fn_t cmp);

#endif /* PROVEN_ALGORITHM_H */
