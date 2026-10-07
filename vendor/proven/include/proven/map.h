#ifndef PROVEN_MAP_H
#define PROVEN_MAP_H

#include <stdalign.h>
#include "proven/types.h"
#include "proven/error.h"
#include "proven/config.h"
#include "proven/allocator.h"
#include "proven/align.h"
#include "proven/u8str.h"

/**
 * @file map.h
 * @brief High-performance, Cache-friendly Open Addressing Hash Map.
 *        Supports zero-allocation U8 String Views or Generic Integer keys directly mapping dense arrays.
 */

typedef enum {
    PROVEN_KEY_TYPE_INT,
    PROVEN_KEY_TYPE_U8_BORROWED, // Borrowed view; caller keeps the bytes alive for the map lifetime
    PROVEN_KEY_TYPE_U8_OWNED     // Copied bytes; the map owns and frees the key storage
} proven_key_type_t;

typedef union {
    proven_size_t id;             // For Integer Keys
    proven_u8str_view_t str;      // For String Keys
} proven_map_key_t;

typedef struct {
    proven_allocator_t alloc;
    proven_mem_mut_t internal;    // Linear Open Addressing bucket array
    proven_size_t len;            // Number of occupied elements
    proven_size_t used;           // Occupied + Tombstones for true load factor calc
    proven_size_t cap;            // Capacity (always a power of 2)
    proven_size_t elem_size;      // Size of the mapped value structural footprint
    proven_size_t align;          // Alignment of the mapped value
    proven_size_t bucket_stride;  // Internal mathematically aligned bucket hopping metric
    proven_size_t payload_offset; // Cached offset to the start of the value payload
    proven_key_type_t key_type;   // Tracks configured map mode

    /**
     * @brief Hash untrusted string keys with a keyed, unpredictable hash (the default), or
     *        with fast FNV-1a because you trust the keys.
     *
     * false (the default from proven_map_create): string keys are hashed with SipHash-2-4
     * under a per-process random key, so an attacker who controls the keys cannot compute
     * collisions and flood one bucket - the HashDoS attack that turns O(1) into O(n^2).
     *
     * true (from proven_map_create_trusted): string keys use FNV-1a, which is faster and
     * needs no randomness, and is the right choice when every key comes from your own code.
     *
     * Integer keys follow the same choice: SipHash-1-3 of the key's 8 bytes under the same
     * secret by default, a public bit-mix finaliser when trusted. (Before RFC-0009 S-001 they
     * always used the finaliser, whose inverse is public.) Read-only; set it by choosing
     * which create function you call.
     */
    bool trusted_keys;
} proven_map_t;

typedef struct {
    proven_err_t err;
    proven_map_t value;
} proven_result_map_t;

// -------------------------------------------------------------
// Type-Agnostic Core C API
// -------------------------------------------------------------

/**
 * @brief Create a map. String keys are hashed with a keyed, HashDoS-resistant hash by
 *        default; see proven_map_create_trusted for the fast path when you trust the keys.
 *
 * @note The keyed hash draws a per-process secret from the OS CSPRNG the first time a key is
 *       hashed. Integer keys are keyed too (SipHash-1-3), since ids from a request are as
 *       attacker-chosen as strings. On a freestanding target, which has no CSPRNG, keys fall
 *       back to the unkeyed functions and are NOT HashDoS-resistant - there is no attacker
 *       model on a target with no OS, and no entropy to key with.
 */
[[nodiscard]] proven_result_map_t proven_map_create(proven_allocator_t alloc, proven_size_t init_cap, proven_key_type_t key_type, proven_size_t elem_size, proven_size_t align);

/**
 * @brief Create a map that hashes string keys with fast FNV-1a, for keys you trust.
 *
 * Identical to proven_map_create except that keys are hashed without the secret: string keys
 * with FNV-1a, integer keys with a bit-mix finaliser. Use it when every key is chosen by your own program - build a
 * lookup table of your own identifiers, dedup a batch of your own blobs - where the extra
 * cost of a keyed hash buys nothing because there is no adversary choosing the keys.
 *
 * @warning Do NOT use this for keys that come from outside your program - request headers,
 *          file names you did not create, network data. That is exactly the HashDoS-able
 *          case proven_map_create defends against, and this opts out of the defence.
 */
[[nodiscard]] proven_result_map_t proven_map_create_trusted(proven_allocator_t alloc, proven_size_t init_cap, proven_key_type_t key_type, proven_size_t elem_size, proven_size_t align);

/**
 * @brief Validates the structural integrity of the public map fields.
 */
[[nodiscard]] bool proven_map_is_valid(const proven_map_t *map);

/**
 * @brief The 64-bit hash this map computes for `key` - the actual function it uses to place
 *        the key, exposed so you can inspect a table's distribution and so the keyed-vs-fast
 *        choice is observable rather than a claim.
 *
 * For a default (untrusted) map this is keyed SipHash (2-4 for strings, 1-3 for integers);
 * for a trusted one it is FNV-1a for strings and the bit-mix finaliser for integers. The map hashes into its bucket
 * array by masking this value, so a poor spread here is a poor spread there.
 */
[[nodiscard]] proven_u64 proven_map_hash(const proven_map_t *map, proven_map_key_t key);

/**
 * @brief Pre-allocates memory for the map to reach at least `new_cap` capacity.
 * Useful when working with arena allocators to prevent dead storage from reallocations.
 */
[[nodiscard]] proven_err_t proven_map_reserve(proven_map_t *map, proven_size_t new_cap);

/**
 * @brief Alias for proven_map_create that highlights the intentional capability allocation.
 */
#define proven_map_create_with_capacity(alloc, init_cap, key_type, elem_size, align) \
    proven_map_create(alloc, init_cap, key_type, elem_size, align)

/*
 * Sets a map value using a separate scratch allocator for temporary work buffers.
 *
 * Persistent map storage, including bucket arrays allocated during rehash,
 * still uses map->alloc. The scratch allocator is used only for temporary
 * buffers needed during this call, primarily to preserve an element that
 * aliases the map's current storage before a rehash.
 */
[[nodiscard]] proven_err_t proven_map_set_with_scratch(proven_map_t *map, proven_map_key_t key, const void *element, proven_allocator_t scratch);

[[nodiscard]] proven_err_t proven_map_set(proven_map_t *map, proven_map_key_t key, const void *element);

/**
 * @brief Inserts or replaces a value in an owned U8-string-key map.
 *
 * The map duplicates the key bytes into map-owned storage on insert.
 * The caller may release or reuse the source buffer after the call returns.
 */
[[nodiscard]] proven_err_t proven_map_set_u8_owned(proven_map_t *map, proven_u8str_view_t key, const void *element);

/**
 * @brief A pointer into the container's storage. It dies the next time the container grows.
 *
 * @warning The returned pointer is INVALIDATED by any operation that may reallocate -
 *          push, reserve, set, append, an insert that triggers a rehash. Using it
 *          afterwards is a use-after-free, and the sanitizers will say so. Hold the index
 *          or the key, not the pointer, across a mutation.
 */
[[nodiscard]] void* proven_map_get_mut(proven_map_t *map, proven_map_key_t key);
[[nodiscard]] const void* proven_map_get(const proven_map_t *map, proven_map_key_t key);

[[nodiscard]] proven_err_t proven_map_remove(proven_map_t *map, proven_map_key_t key);

void proven_map_destroy(proven_map_t *map);

/**
 * @brief The number of entries in the map. 0 for NULL.
 */
[[nodiscard]] proven_size_t proven_map_len(const proven_map_t *map);

/**
 * @brief A cursor over a map's entries, in bucket order (not insertion order, and not
 *        stable from one process to the next for a keyed map).
 *
 * Fill it with proven_map_iter_init, then call proven_map_iter_next until it returns
 * PROVEN_ERR_EOF. The fields are the iterator's own; do not set them.
 */
typedef struct {
    proven_map_t     *map;
    proven_size_t     next;      /* the bucket to look at next */
    const void       *storage;   /* the bucket array when the walk started */
    proven_size_t     cap;
} proven_map_iter_t;

/**
 * @brief Start a walk over every entry of `map`.
 */
[[nodiscard]] proven_map_iter_t proven_map_iter_init(proven_map_t *map);

/**
 * @brief The next entry: its key and a pointer to its value. PROVEN_ERR_EOF after the last.
 *
 * Allowed during a walk: removing any entry, including the one just returned (proven_map_remove
 * leaves the slot in place; a removed entry not yet reached is not returned), and changing a
 * value through `*out_value` or by proven_map_set on a key that is already there.
 *
 * Not allowed: adding a new key, proven_map_reserve, or anything else that can grow or rehash
 * the map - entries move, so the walk could skip or repeat them. The iterator notices that the
 * bucket array changed and returns PROVEN_ERR_INVALID_STATE instead of reading it; start a new
 * walk. (proven_map_destroy during a walk is a use-after-free like any other.)
 *
 * @param out_key   the entry's key. For a string-key map the view points into the map (owned
 *                  keys) or at the caller's bytes (borrowed keys); an owned key's bytes are
 *                  freed when that entry is removed.
 * @param out_value the entry's value, in the map's storage; same lifetime rules as
 *                  proven_map_get_mut. Either out pointer may be NULL.
 */
[[nodiscard]] proven_err_t proven_map_iter_next(proven_map_iter_t *it, proven_map_key_t *out_key, void **out_value);

// -------------------------------------------------------------
// Type-Safe Strict Macro Wrappers
// -------------------------------------------------------------

/*
 * The wrappers below take a key view and a value as macro arguments. *
 * MACRO ARGUMENTS AND COMPOUND LITERALS (RFC-0009 X-008). These are function-like macros, and
 * the preprocessor splits arguments at every top-level comma - including the one inside a
 * compound literal's braces. `(proven_u8str_view_t){ p, n }` passed as an argument becomes two
 * arguments, and the error names a macro you did not write. Pass a variable or a PROVEN_LIT,
 * or put the compound literal in parentheses: `((proven_u8str_view_t){ p, n })`.
 */

#define PROVEN_MAP_INIT_INT(alloc, type, init_cap) \
    proven_map_create((alloc), (init_cap), PROVEN_KEY_TYPE_INT, sizeof(type), alignof(type))

#define PROVEN_MAP_INIT_U8_BORROWED(alloc, type, init_cap) \
    proven_map_create((alloc), (init_cap), PROVEN_KEY_TYPE_U8_BORROWED, sizeof(type), alignof(type))

#define PROVEN_MAP_INIT_U8_OWNED(alloc, type, init_cap) \
    proven_map_create((alloc), (init_cap), PROVEN_KEY_TYPE_U8_OWNED, sizeof(type), alignof(type))

#define PROVEN_MAP_SET_INT(map_ptr, int_key, type, value) \
    proven_map_set((map_ptr), (proven_map_key_t){ .id = (proven_size_t)(int_key) }, (type[]){(value)})

#define PROVEN_MAP_SET_WITH_SCRATCH_INT(map_ptr, int_key, type, value, scratch) \
    proven_map_set_with_scratch( \
        (map_ptr), \
        (proven_map_key_t){ .id = (proven_size_t)(int_key) }, \
        (type[]){(value)}, \
        (scratch) \
    )

#define PROVEN_MAP_SET_U8_BORROWED(map_ptr, u8_view, type, value) \
    proven_map_set((map_ptr), (proven_map_key_t){ .str = (u8_view) }, (type[]){(value)})

#define PROVEN_MAP_SET_U8_OWNED(map_ptr, u8_view, type, value) \
    proven_map_set_u8_owned((map_ptr), (u8_view), (type[]){(value)})

#define PROVEN_MAP_SET_WITH_SCRATCH_U8_BORROWED(map_ptr, u8_view, type, value, scratch) \
    proven_map_set_with_scratch( \
        (map_ptr), \
        (proven_map_key_t){ .str = (u8_view) }, \
        (type[]){(value)}, \
        (scratch) \
    )

#define PROVEN_MAP_GET_INT(map_ptr, type, int_key) \
    ((const type*)proven_map_get((map_ptr), (proven_map_key_t){ .id = (proven_size_t)(int_key) }))

#define PROVEN_MAP_GET_U8_BORROWED(map_ptr, type, u8_view) \
    ((const type*)proven_map_get((map_ptr), (proven_map_key_t){ .str = (u8_view) }))

#define PROVEN_MAP_GET_U8_OWNED(map_ptr, type, u8_view) \
    ((const type*)proven_map_get((map_ptr), (proven_map_key_t){ .str = (u8_view) }))

#define PROVEN_MAP_GET_MUT_INT(map_ptr, type, int_key) \
    ((type*)proven_map_get_mut((map_ptr), (proven_map_key_t){ .id = (proven_size_t)(int_key) }))

#define PROVEN_MAP_GET_MUT_U8_BORROWED(map_ptr, type, u8_view) \
    ((type*)proven_map_get_mut((map_ptr), (proven_map_key_t){ .str = (u8_view) }))

#define PROVEN_MAP_GET_MUT_U8_OWNED(map_ptr, type, u8_view) \
    ((type*)proven_map_get_mut((map_ptr), (proven_map_key_t){ .str = (u8_view) }))

#define PROVEN_MAP_REMOVE_INT(map_ptr, int_key) \
    proven_map_remove((map_ptr), (proven_map_key_t){ .id = (proven_size_t)(int_key) })

#define PROVEN_MAP_REMOVE_U8_BORROWED(map_ptr, u8_view) \
    proven_map_remove((map_ptr), (proven_map_key_t){ .str = (u8_view) })

#define PROVEN_MAP_REMOVE_U8_OWNED(map_ptr, u8_view) \
    proven_map_remove((map_ptr), (proven_map_key_t){ .str = (u8_view) })

#define PROVEN_MAP_DESTROY(map_ptr) \
    proven_map_destroy(map_ptr)


#endif /* PROVEN_MAP_H */
