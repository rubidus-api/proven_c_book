#ifndef PROVEN_LIST_H
#define PROVEN_LIST_H

#include "proven/types.h"

/**
 * @file list.h
 * @brief High-performance, intrusive doubly-linked list inspired by Linux Kernel.
 *        Guarantees zero-allocation (OOM-immune) linkage and perfect cache locality.
 */

// -------------------------------------------------------------
// Core Node & List Structures
// -------------------------------------------------------------

typedef struct proven_list_node_t {
    struct proven_list_node_t *next;
    struct proven_list_node_t *prev;
} proven_list_node_t;

typedef struct {
    proven_list_node_t head;
} proven_list_t;

// -------------------------------------------------------------
// C API (Structure Mutations)
// -------------------------------------------------------------

/**
 * @brief Initializes the intrusive list safely constructing the internal circular boundary.
 */
static inline void proven_list_init(proven_list_t *list) {
    if (list) {
        list->head.next = &list->head;
        list->head.prev = &list->head;
    }
}

/**
 * @brief Embeds the designated node right after the specified linked target.
 */
static inline void proven_list_insert_after(proven_list_node_t *target, proven_list_node_t *node) {
    if (!target || !node) return;
    node->next = target->next;
    node->prev = target;
    target->next->prev = node;
    target->next = node;
}

/**
 * @brief Pushes the node directly sequentially to the back tail of the circular list structure.
 */
static inline void proven_list_push_back(proven_list_t *list, proven_list_node_t *node) {
    if (list) proven_list_insert_after(list->head.prev, node);
}

/**
 * @brief Detaches the node from the chain boundary healing the surrounding linkage gap.
 */
static inline void proven_list_remove(proven_list_node_t *node) {
    if (!node || !node->next || !node->prev) return;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    // Poison pointers conservatively avoiding dangling resolutions
    node->next = (proven_list_node_t *)0;
    node->prev = (proven_list_node_t *)0;
}

/**
 * @brief Validates boolean truth of an empty list state evaluating inner pointer circle equality.
 */
static inline int proven_list_is_empty(const proven_list_t *list) {
    return list ? (list->head.next == &list->head) : 1;
}

// -------------------------------------------------------------
// C23 Strict & Standard Reverse Calculation Macros
// -------------------------------------------------------------

/**
 * @brief C Compiler Intrinsic Type-Casting Extraction Macro (Linux container_of concept)
 * Safely reverse-calculates mathematical pointer offsets translating embedded node positions back to user Parent Structs bounds.
 */
#define PROVEN_CONTAINER_OF(ptr, type, member) \
    ((type *)((proven_byte_t *)(ptr) - offsetof(type, member)))

/**
 * @brief Iterable loop macro. Traverses intrusive list links effectively skipping parent list encapsulator bounds.
 */
#define PROVEN_LIST_FOR_EACH(iter, list) \
    for ((iter) = (list)->head.next; (iter) != &(list)->head; (iter) = (iter)->next)

/**
 * @brief Safe iterable loop macro explicitly guarding deletion mutations internally storing trailing states implicitly.
 */
#define PROVEN_LIST_FOR_EACH_SAFE(iter, safe_next, list) \
    for ((iter) = (list)->head.next, (safe_next) = (iter)->next; \
         (iter) != &(list)->head; \
         (iter) = (safe_next), (safe_next) = (iter)->next)

/**
 * @brief Hybrid macro mapping structural references instantly yielding parsed container entries.
 */
#define PROVEN_LIST_ENTRY(ptr, type, member) \
    PROVEN_CONTAINER_OF(ptr, type, member)

#endif /* PROVEN_LIST_H */
