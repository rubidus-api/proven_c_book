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
     * collisions and flood one bucket - the HashDoS attack that turns O(1) into O(n²).
     *
     * true (from proven_map_create_trusted): string keys use FNV-1a, which is faster and
     * needs no randomness, and is the right choice when every key comes from your own code.
     *
     * Integer keys ignore this - they always use a bit-mix finaliser. Read-only; set it by
     * choosing which create function you call.
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
 * @note The keyed hash draws a per-process secret from the OS CSPRNG the first time a
 *       string-key map is created. On a freestanding target, which has no CSPRNG, string
 *       keys fall back to FNV-1a and are NOT HashDoS-resistant - there is no attacker model
 *       on a target with no OS, and no entropy to key with.
 */
[[nodiscard]] proven_result_map_t proven_map_create(proven_allocator_t alloc, proven_size_t init_cap, proven_key_type_t key_type, proven_size_t elem_size, proven_size_t align);

/**
 * @brief Create a map that hashes string keys with fast FNV-1a, for keys you trust.
 *
 * Identical to proven_map_create except that string keys are hashed with unkeyed FNV-1a
 * instead of keyed SipHash. Use it when every key is chosen by your own program - build a
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
 * For a default (untrusted) string-key map this is keyed SipHash; for a trusted one it is
 * FNV-1a; for an integer-key map it is the bit-mix finaliser. The map hashes into its bucket
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

// -------------------------------------------------------------
// Type-Safe Strict Macro Wrappers
// -------------------------------------------------------------

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
