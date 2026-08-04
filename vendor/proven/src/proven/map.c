#include "proven/map.h"
#include "proven/hash.h"
#ifndef PROVEN_FREESTANDING
#include "proven/random.h"
#include "../../platform/proven_sys_thread.h"
#endif
#include "proven/memory.h"
#include "proven_internal_memrange.h"
#include "../../platform/proven_sys_mem.h"
#include <stdalign.h>
#include <stdatomic.h>

#define BUCKET_EMPTY 0
#define BUCKET_OCCUPIED 1
#define BUCKET_TOMBSTONE 2

typedef struct {
    proven_byte_t state;
    // Padding logic handled correctly via proven_mem_align_up mathematics on payload extraction
    proven_map_key_t key;
} proven_map_bucket_header_t;

// SplitMix64 rapid avalanche integer mixer
static proven_size_t hash_int(proven_size_t key) {
#if UINTPTR_MAX == 0xffffffff
    // 32-bit fallback
    proven_size_t z = key;
    z ^= z >> 16;
    z *= 0x85ebca6b;
    z ^= z >> 13;
    z *= 0xc2b2ae35;
    z ^= z >> 16;
    return z;
#else
    proven_size_t z = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
#endif
}

#ifndef PROVEN_FREESTANDING
/*
 * One per-process SipHash key, drawn from the OS CSPRNG the first time any keyed map hashes
 * a string. Every keyed map shares it (an attacker does not know it, which is the whole
 * point), and it is seeded EXACTLY once: two threads racing the first hash must end up using
 * the SAME key, or a key placed under one and looked up under another would be unfindable.
 *
 * The CAS on `state` grants exactly one thread the right to seed; the rest wait for it to
 * publish. 0 = untouched, 1 = being seeded, 2 = ready.
 */
static proven_byte_t g_map_key[16];
static _Atomic int    g_map_key_state = 0;

static void map_ensure_key(void) {
    if (atomic_load_explicit(&g_map_key_state, memory_order_acquire) == 2) return;

    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_map_key_state, &expected, 1,
                                                memory_order_acq_rel, memory_order_acquire)) {
        if (!proven_random_bytes(g_map_key, sizeof g_map_key)) {
            /*
             * The OS CSPRNG failed - essentially impossible on a hosted platform, since it
             * falls back to /dev/urandom. If it somehow does, derive a key that is at least
             * not a constant, so the map stays keyed rather than silently becoming FNV: the
             * address of a static (ASLR) and the address of this frame, mixed. Weaker than a
             * real secret, stronger than "everyone shares the same key".
             */
            proven_uintptr_t a = (proven_uintptr_t)(void *)&g_map_key;
            proven_uintptr_t b = (proven_uintptr_t)(void *)&expected;
            proven_u64 mix = (proven_u64)a * 0x9e3779b97f4a7c15ull + (proven_u64)b;
            for (int i = 0; i < 16; ++i) { mix ^= mix >> 29; mix *= 0xbf58476d1ce4e5b9ull; g_map_key[i] = (proven_byte_t)(mix >> 56); }
        }
        atomic_store_explicit(&g_map_key_state, 2, memory_order_release);
    } else {
        while (atomic_load_explicit(&g_map_key_state, memory_order_acquire) != 2) {
            proven_sys_thread_yield();
        }
    }
}
#endif

/* The full 64-bit hash for a key, honouring the map's trusted/keyed choice. */
static proven_u64 map_hash64(const proven_map_t *map, proven_map_key_t key) {
    if (map->key_type == PROVEN_KEY_TYPE_INT) {
        return (proven_u64)hash_int(key.id);
    }
    proven_mem_view_t kv = { .ptr = key.str.ptr, .size = key.str.size };
#ifndef PROVEN_FREESTANDING
    if (!map->trusted_keys) {
        map_ensure_key();
        return proven_hash_keyed(kv, g_map_key);
    }
#endif
    /* proven_hash_bytes is the public 64-bit FNV-1a, and unlike the internal duplicate it
     * removed it guards a {NULL, size>0} key rather than dereferencing it - so a malformed
     * key handed to proven_map_hash behaves the same on a trusted map as on a default one. */
    return proven_hash_bytes(kv);
}

static proven_size_t get_hash(const proven_map_t *map, proven_map_key_t key) {
    return (proven_size_t)map_hash64(map, key);
}

static int keys_equal(proven_key_type_t type, proven_map_key_t a, proven_map_key_t b) {
    if (type == PROVEN_KEY_TYPE_INT) {
        return a.id == b.id;
    } else {
        return proven_u8str_view_eq(a.str, b.str);
    }
}

static void map_release_owned_key(proven_map_t *map, proven_map_bucket_header_t *hdr) {
    if (!map || !hdr || map->key_type != PROVEN_KEY_TYPE_U8_OWNED) {
        return;
    }
    if (hdr->state == BUCKET_OCCUPIED && hdr->key.str.ptr && map->alloc.free_fn) {
        map->alloc.free_fn(map->alloc.ctx, (void *)hdr->key.str.ptr);
    }
    hdr->key.str.ptr = NULL;
    hdr->key.str.size = 0;
}

static proven_err_t map_store_key(proven_map_t *map, proven_map_bucket_header_t *hdr, proven_map_key_t key, bool duplicate_owned) {
    hdr->key = key;
    if (map->key_type != PROVEN_KEY_TYPE_U8_OWNED || !duplicate_owned) {
        return PROVEN_OK;
    }
    if (key.str.size == 0) {
        hdr->key.str.ptr = NULL;
        return PROVEN_OK;
    }
    proven_result_mem_mut_t key_alloc = map->alloc.alloc_fn(map->alloc.ctx, key.str.size, 1);
    if (!PROVEN_IS_OK(key_alloc.err)) {
        hdr->key.str.ptr = NULL;
        hdr->key.str.size = 0;
        return key_alloc.err;
    }
    proven_sys_mem_copy(key_alloc.value.ptr, key.str.ptr, key.str.size);
    hdr->key.str.ptr = key_alloc.value.ptr;
    return PROVEN_OK;
}

static proven_err_t map_insert_no_grow(proven_map_t *map, proven_map_key_t key, const void *element, bool duplicate_owned);
static void* map_find_payload_mut(proven_map_t *map, proven_map_key_t key);
static const void* map_find_payload_const(const proven_map_t *map, proven_map_key_t key);

typedef struct {
    proven_err_t err;
    proven_size_t value;
} proven_map_result_size_t;

// Ensure capacity is a valid mathematical power of 2 for rapid modulo wrapping
static proven_map_result_size_t next_pow2(proven_size_t v) {
    proven_map_result_size_t res = {0};
    if (v < 8) {
        res.value = 8;
        return res;
    }

    if (v > ((proven_size_t)1 << (sizeof(proven_size_t) * 8 - 1))) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#if UINTPTR_MAX == 0xffffffffffffffff
    v |= v >> 32;
#endif
    v++;
    res.value = v;
    return res;
}

static proven_result_mem_mut_t alloc_buckets(proven_allocator_t alloc, proven_size_t cap, proven_size_t bucket_stride, proven_size_t align) {
    // Determine maximum boundary limit unifying payload alignments overriding structural metadata limitations
    proven_size_t req_align = align > alignof(proven_map_bucket_header_t) ? align : alignof(proven_map_bucket_header_t);
    
    proven_size_t total_size;
    if (PROVEN_CKD_MUL(&total_size, cap, bucket_stride)) {
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_OVERFLOW };
    }
    
    proven_result_mem_mut_t res = alloc.alloc_fn(alloc.ctx, total_size, req_align);
    
    if (PROVEN_IS_OK(res.err)) {
        // Zero-initialization enforcing Empty states implicitly across fresh blocks
        proven_sys_mem_zero(res.value.ptr, total_size);
    }
    return res;
}

static bool map_key_type_is_valid(proven_key_type_t key_type) {
    return key_type == PROVEN_KEY_TYPE_INT ||
           key_type == PROVEN_KEY_TYPE_U8_BORROWED ||
           key_type == PROVEN_KEY_TYPE_U8_OWNED;
}

proven_result_map_t proven_map_create(proven_allocator_t alloc, proven_size_t init_cap, proven_key_type_t key_type, proven_size_t elem_size, proven_size_t align) {
    proven_result_map_t res = {0};
    
    if (elem_size == 0 || !proven_alloc_is_valid(alloc) || !proven_is_pow2(align) || !map_key_type_is_valid(key_type)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_map_result_size_t cap_res = next_pow2(init_cap);
    if (!PROVEN_IS_OK(cap_res.err)) {
        res.err = cap_res.err;
        return res;
    }
    proven_size_t actual_cap = cap_res.value;
    
    // Calculate mathematically exact physical bounding jumps preventing hardware faults
    proven_size_t req_align = align > alignof(proven_map_bucket_header_t) ? align : alignof(proven_map_bucket_header_t);
    proven_size_t payload_offset = proven_mem_align_up(sizeof(proven_map_bucket_header_t), req_align);
    if (payload_offset == 0) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    
    proven_size_t stride_base;
    if (PROVEN_CKD_ADD(&stride_base, payload_offset, elem_size)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    
    proven_size_t bucket_stride = proven_mem_align_up(stride_base, req_align);
    if (bucket_stride == 0) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    proven_mem_mut_t block = {0};
    
    if (actual_cap > 0) {
        proven_result_mem_mut_t am = alloc_buckets(alloc, actual_cap, bucket_stride, align);
        if (!PROVEN_IS_OK(am.err)) {
            res.err = am.err;
            return res;
        }
        block = am.value;
    }

    proven_map_t map = {
        .alloc = alloc,
        .internal = block,
        .len = 0,
        .used = 0,
        .cap = actual_cap,
        .elem_size = elem_size,
        .align = align,
        .bucket_stride = bucket_stride,
        .payload_offset = payload_offset,
        .key_type = key_type,
        .trusted_keys = false
    };

    res.err = PROVEN_OK;
    res.value = map;
    return res;
}

/*
 * Contract first, implementation next (docs/TESTING.md §5.1). These are the stubs the
 * keyed-hash test was written against; proven_map_create_trusted is real (it only flips a
 * flag), but the default create still hashes strings with FNV, so the test's assertion that
 * a default map's hash differs from FNV lands RED here and goes green in the next commit.
 */
proven_result_map_t proven_map_create_trusted(proven_allocator_t alloc, proven_size_t init_cap, proven_key_type_t key_type, proven_size_t elem_size, proven_size_t align) {
    proven_result_map_t r = proven_map_create(alloc, init_cap, key_type, elem_size, align);
    if (PROVEN_IS_OK(r.err)) r.value.trusted_keys = true;
    return r;
}

proven_u64 proven_map_hash(const proven_map_t *map, proven_map_key_t key) {
    if (!map) return 0;
    /* stub: report the same non-keyed hash the internals currently use */
    return map_hash64(map, key);
}

bool proven_map_is_valid(const proven_map_t *map) {
    if (!map) return false;
    if (map->elem_size == 0 || !proven_is_pow2(map->align)) return false;
    if (map->len > map->used) return false;
    if (map->used > map->cap) return false;
    if (map->cap > 0 && !proven_is_pow2(map->cap)) return false;
    if (map->cap > 0 && !map->internal.ptr) return false;
    if (map->cap == 0 && map->internal.ptr) return false;
    if (map->cap == 0 && map->internal.size != 0) return false;
    
    if (!map_key_type_is_valid(map->key_type)) return false;
    if (!proven_alloc_is_valid(map->alloc)) return false;
    
    proven_size_t req_align = map->align > alignof(proven_map_bucket_header_t) ? map->align : alignof(proven_map_bucket_header_t);
    proven_size_t payload_offset = proven_mem_align_up(sizeof(proven_map_bucket_header_t), req_align);
    if (payload_offset == 0 || payload_offset != map->payload_offset) return false;
    
    proven_size_t stride_base;
    if (PROVEN_CKD_ADD(&stride_base, payload_offset, map->elem_size)) return false;
    proven_size_t bucket_stride = proven_mem_align_up(stride_base, req_align);
    if (bucket_stride == 0 || bucket_stride != map->bucket_stride) return false;
    if (map->cap > 0) {
        proven_size_t expected_size;
        if (PROVEN_CKD_MUL(&expected_size, map->cap, map->bucket_stride)) return false;
        if (map->internal.size != expected_size) return false;
    }
    
    return true;
}

static proven_err_t map_insert_no_grow(proven_map_t *map, proven_map_key_t key, const void *element, bool duplicate_owned);

static proven_err_t map_rehash_target(proven_map_t *map, proven_size_t target_cap) {
    proven_size_t new_cap = target_cap;
    if (!proven_is_pow2(new_cap)) {
        proven_size_t p = 1;
        while (p < new_cap) {
            if (PROVEN_CKD_MUL(&p, p, 2)) return PROVEN_ERR_OVERFLOW;
        }
        new_cap = p;
    }

    proven_result_mem_mut_t new_block_res = alloc_buckets(map->alloc, new_cap, map->bucket_stride, map->align);
    
    if (!PROVEN_IS_OK(new_block_res.err)) return new_block_res.err;
    
    proven_byte_t *old_ptr = map->internal.ptr;
    proven_size_t old_cap = map->cap;
    
    // Create new temporary map
    proven_map_t new_map = *map;
    new_map.internal = new_block_res.value;
    new_map.cap = new_cap;
    new_map.len = 0;
    new_map.used = 0;
    
    // Migrate populated elements
    for (proven_size_t i = 0; i < old_cap; ++i) {
        proven_size_t offset;
        if (PROVEN_CKD_MUL(&offset, i, map->bucket_stride)) {
            map->alloc.free_fn(map->alloc.ctx, new_block_res.value.ptr);
            return PROVEN_ERR_OVERFLOW;
        }
        proven_map_bucket_header_t *old_hdr = (proven_map_bucket_header_t *)(old_ptr + offset);
        if (old_hdr->state == BUCKET_OCCUPIED) {
            void *old_payload = (void *)((proven_byte_t *)old_hdr + map->payload_offset);
            proven_err_t err = map_insert_no_grow(&new_map, old_hdr->key, old_payload, false);
            if (!PROVEN_IS_OK(err)) {
                map->alloc.free_fn(map->alloc.ctx, new_block_res.value.ptr);
                return err;
            }
        }
    }
    
    // Destroy obsolete block smoothly
    if (old_ptr) {
        map->alloc.free_fn(map->alloc.ctx, old_ptr);
    }
    
    *map = new_map;
    return PROVEN_OK;
}

static proven_err_t map_rehash(proven_map_t *map) {
    /*
     * Grow only when the LIVE set needs the room. Otherwise rehash at the same capacity,
     * which reclaims every tombstone for the same one walk.
     *
     * This used to double unconditionally, and `used` counts tombstones as well as live
     * entries - it has to, they still occupy a probe slot. So any workload with churn and
     * a bounded live set - a cache, a session table, a work queue: insert, remove, insert,
     * remove - drove `used` back to the three-quarter threshold over and over while `len`
     * stood still, and the table doubled every time. Measured: 100 live entries and two
     * million operations produced a capacity of 1,048,576 and 33 MB held. It is not a
     * leak - every byte is reachable and freed at destroy - which is exactly why nothing
     * caught it. A long-running service just grows until it dies.
     *
     * Amortisation still holds: after an in-place rehash `used == len < cap/2`, so at
     * least cap/4 more inserts are needed before the threshold is reached again.
     */
    proven_size_t new_cap = map->cap;
    if (map->len >= map->cap / 2u) {
        if (PROVEN_CKD_MUL(&new_cap, map->cap, 2)) {
            return PROVEN_ERR_OVERFLOW;
        }
    }
    return map_rehash_target(map, new_cap);
}

proven_err_t proven_map_reserve(proven_map_t *map, proven_size_t new_cap) {
    if (!proven_map_is_valid(map)) return PROVEN_ERR_INVALID_ARG;
    if (new_cap <= map->cap) return PROVEN_OK;
    return map_rehash_target(map, new_cap);
}

static bool map_key_is_valid(const proven_map_t *map, proven_key_type_t type, proven_map_key_t key);

static proven_err_t map_insert_no_grow(proven_map_t *map, proven_map_key_t key, const void *element, bool duplicate_owned) {
    if (!map || !element) return PROVEN_ERR_INVALID_ARG;
    if (!map_key_is_valid(map, map->key_type, key)) return PROVEN_ERR_INVALID_ARG;
    proven_size_t hash = get_hash(map, key);
    proven_size_t idx = hash & (map->cap - 1); 
    
    proven_map_bucket_header_t *first_tombstone = (void*)0;

    for (proven_size_t i = 0; i < map->cap; ++i) {
        proven_size_t offset;
        if (PROVEN_CKD_MUL(&offset, idx, map->bucket_stride)) return PROVEN_ERR_OVERFLOW;
        proven_map_bucket_header_t *hdr = (proven_map_bucket_header_t *)(map->internal.ptr + offset);
        
        if (hdr->state == BUCKET_EMPTY) {
            proven_map_bucket_header_t *target = first_tombstone ? first_tombstone : hdr;
            target->state = BUCKET_OCCUPIED;
            proven_err_t key_err = map_store_key(map, target, key, duplicate_owned);
            if (!PROVEN_IS_OK(key_err)) {
                target->state = first_tombstone ? BUCKET_TOMBSTONE : BUCKET_EMPTY;
                return key_err;
            }
            proven_byte_t *dst = (proven_byte_t *)target + map->payload_offset;
            proven_sys_mem_move(dst, element, map->elem_size);
            map->len++;
            if (!first_tombstone) map->used++;
            return PROVEN_OK;
        } else if (hdr->state == BUCKET_OCCUPIED) {
            if (keys_equal(map->key_type, hdr->key, key)) {
                proven_byte_t *dst = (proven_byte_t *)hdr + map->payload_offset;
                proven_sys_mem_move(dst, element, map->elem_size);
                return PROVEN_OK;
            }
        } else if (hdr->state == BUCKET_TOMBSTONE) {
            if (!first_tombstone) first_tombstone = hdr; 
        }
        idx = (idx + 1) & (map->cap - 1); 
    }
    
    if (first_tombstone) {
        first_tombstone->state = BUCKET_OCCUPIED;
        proven_err_t key_err = map_store_key(map, first_tombstone, key, duplicate_owned);
        if (!PROVEN_IS_OK(key_err)) {
            first_tombstone->state = BUCKET_TOMBSTONE;
            return key_err;
        }
        proven_byte_t *dst = (proven_byte_t *)first_tombstone + map->payload_offset;
        proven_sys_mem_move(dst, element, map->elem_size);
        map->len++;
        return PROVEN_OK;
    }
    
    return PROVEN_ERR_OUT_OF_BOUNDS; 
}

static bool map_key_is_valid(const proven_map_t *map, proven_key_type_t type, proven_map_key_t key) {
    (void)map;  /* only read by the hardened overlap check below; compiled out on -DNDEBUG non-hardened builds */
    if (type == PROVEN_KEY_TYPE_INT) return true;
    if (type == PROVEN_KEY_TYPE_U8_BORROWED || type == PROVEN_KEY_TYPE_U8_OWNED) {
        if (key.str.size != 0 && key.str.ptr == NULL) {
            return false;
        }
#if PROVEN_HARDENED || !defined(NDEBUG)
        if (map && map->internal.ptr && map->internal.size > 0) {
            if (proven_range_contains_ptr(map->internal.ptr, map->internal.size, key.str.ptr, key.str.size, NULL)) {
                return false;
            }
        }
#endif
        return true;
    }
    return false;
}

proven_err_t proven_map_set_with_scratch(proven_map_t *map, proven_map_key_t key, const void *element, proven_allocator_t scratch) {
    if (!proven_map_is_valid(map) || !element || map->cap == 0) return PROVEN_ERR_INVALID_ARG;
    if (!map_key_is_valid(map, map->key_type, key)) return PROVEN_ERR_INVALID_ARG;
    
    if (!scratch.alloc_fn || !scratch.free_fn) {
        scratch = map->alloc;
    }
    
    proven_size_t threshold;
    if (PROVEN_CKD_MUL(&threshold, map->cap, 3)) threshold = map->cap; 
    threshold /= 4;
    
    // Ensure threshold is at least 1 if cap > 1 to avoid premature rehashing on empty maps
    if (map->cap > 1 && threshold == 0) threshold = 1;

    const void *insert_elem = element;

    if (map->used >= threshold) {
        /* Only probe for an existing key when we are about to grow. Overwriting
         * an existing key needs no new slot, so finding it here saves a rehash.
         *
         * Below the threshold this probe is pure waste: map_insert_no_grow
         * already overwrites a key it finds, so the old code walked the same
         * chain twice for every set. */
        void *existing = map_find_payload_mut(map, key);
        if (existing) {
            if (existing != element) {
                proven_sys_mem_move(existing, element, map->elem_size);
            }
            return PROVEN_OK;
        }

        proven_bufref_t alias_ref = proven_bufref_capture(
            map->internal.ptr,
            map->internal.size,
            element,
            map->elem_size
        );

        proven_byte_t stack_temp[256];
        proven_byte_t *heap_temp = NULL;
        proven_byte_t *temp = NULL;

        if (alias_ref.valid) {
            if (map->elem_size <= sizeof(stack_temp)) {
                temp = stack_temp;
            } else {
                proven_result_mem_mut_t m = scratch.alloc_fn(scratch.ctx, map->elem_size, map->align);
                if (!PROVEN_IS_OK(m.err)) return m.err;
                heap_temp = m.value.ptr;
                temp = heap_temp;
            }
            proven_sys_mem_copy(temp, element, map->elem_size);
            insert_elem = temp;
        }

        proven_err_t grow_err = map_rehash(map);
        
        if (!PROVEN_IS_OK(grow_err)) {
            if (heap_temp) scratch.free_fn(scratch.ctx, heap_temp);
            return grow_err;
        }
        
        // After rehash, we must insert using the temp buffer if it was aliased
        proven_err_t res = map_insert_no_grow(map, key, insert_elem, map->key_type == PROVEN_KEY_TYPE_U8_OWNED);
        if (heap_temp) scratch.free_fn(scratch.ctx, heap_temp);
        return res;
    }

    return map_insert_no_grow(map, key, insert_elem, map->key_type == PROVEN_KEY_TYPE_U8_OWNED);
}

proven_err_t proven_map_set(proven_map_t *map, proven_map_key_t key, const void *element) {
    /* No validation here: proven_map_set_with_scratch validates the same map on
     * its first line, and reading map->alloc for an invalid map is exactly what
     * that check would have rejected - so read it only after the check passes. */
    if (!map) return PROVEN_ERR_INVALID_ARG;
    return proven_map_set_with_scratch(map, key, element, map->alloc);
}

proven_err_t proven_map_set_u8_owned(proven_map_t *map, proven_u8str_view_t key, const void *element) {
    if (!proven_map_is_valid(map) || map->key_type != PROVEN_KEY_TYPE_U8_OWNED) {
        return PROVEN_ERR_INVALID_ARG;
    }
    return proven_map_set(map, (proven_map_key_t){ .str = key }, element);
}

/*
 * len counts live entries (OCCUPIED buckets).
 * used counts non-EMPTY buckets (OCCUPIED + TOMBSTONE).
 * Therefore, remove() decrements len but MUST NOT decrement used.
 * This ensures that tombstone accumulation triggers rehash when non-empty 
 * bucket ratio exceeds the threshold, maintaining linear search efficiency.
 */
static void* map_find_payload_mut(proven_map_t *map, proven_map_key_t key) {
    if (!map || map->cap == 0) return (void*)0;
    if (!map || !map_key_is_valid(map, map->key_type, key)) return (void*)0;
    
    proven_size_t hash = get_hash(map, key);
    proven_size_t idx = hash & (map->cap - 1);
    
    for (proven_size_t i = 0; i < map->cap; ++i) {
        proven_size_t offset;
        if (PROVEN_CKD_MUL(&offset, idx, map->bucket_stride)) return (void*)0;
        proven_map_bucket_header_t *hdr = (proven_map_bucket_header_t *)(map->internal.ptr + offset);
        
        if (hdr->state == BUCKET_EMPTY) {
            return (void*)0; 
        } else if (hdr->state == BUCKET_OCCUPIED && keys_equal(map->key_type, hdr->key, key)) {
            return (void*)((proven_byte_t *)hdr + map->payload_offset);
        }
        
        idx = (idx + 1) & (map->cap - 1);
    }
    
    return (void*)0;
}

static const void* map_find_payload_const(const proven_map_t *map, proven_map_key_t key) {
    if (!map || map->cap == 0) return (void*)0;
    if (!map || !map_key_is_valid(map, map->key_type, key)) return (void*)0;
    
    proven_size_t hash = get_hash(map, key);
    proven_size_t idx = hash & (map->cap - 1);
    
    for (proven_size_t i = 0; i < map->cap; ++i) {
        proven_size_t offset;
        if (PROVEN_CKD_MUL(&offset, idx, map->bucket_stride)) return (void*)0;
        const proven_map_bucket_header_t *hdr = (const proven_map_bucket_header_t *)(map->internal.ptr + offset);
        
        if (hdr->state == BUCKET_EMPTY) {
            return (void*)0; 
        } else if (hdr->state == BUCKET_OCCUPIED && keys_equal(map->key_type, hdr->key, key)) {
            return (const void*)((const proven_byte_t *)hdr + map->payload_offset);
        }
        
        idx = (idx + 1) & (map->cap - 1);
    }
    
    return (void*)0;
}

void* proven_map_get_mut(proven_map_t *map, proven_map_key_t key) {
    if (!proven_map_is_valid(map)) return (void*)0;
    return map_find_payload_mut(map, key);
}

const void* proven_map_get(const proven_map_t *map, proven_map_key_t key) {
    if (!proven_map_is_valid(map)) return (const void*)0;
    return map_find_payload_const(map, key);
}

proven_err_t proven_map_remove(proven_map_t *map, proven_map_key_t key) {
    if (!proven_map_is_valid(map)) return PROVEN_ERR_INVALID_ARG;
    if (map->cap == 0) return PROVEN_ERR_NOT_FOUND;
    if (!map_key_is_valid(map, map->key_type, key)) return PROVEN_ERR_INVALID_ARG;
    
    proven_size_t hash = get_hash(map, key);
    proven_size_t idx = hash & (map->cap - 1);
    
    for (proven_size_t i = 0; i < map->cap; ++i) {
        proven_size_t offset;
        if (PROVEN_CKD_MUL(&offset, idx, map->bucket_stride)) return PROVEN_ERR_OVERFLOW;
        proven_map_bucket_header_t *hdr = (proven_map_bucket_header_t *)(map->internal.ptr + offset);
        
        if (hdr->state == BUCKET_EMPTY) {
            return PROVEN_ERR_NOT_FOUND;
        } else if (hdr->state == BUCKET_OCCUPIED && keys_equal(map->key_type, hdr->key, key)) {
            map_release_owned_key(map, hdr);
            hdr->state = BUCKET_TOMBSTONE; // Drop Tombstone correctly maintaining ongoing open chaining algorithms mathematically!
            map->len--;
            return PROVEN_OK;
        }
        
        idx = (idx + 1) & (map->cap - 1);
    }
    return PROVEN_ERR_NOT_FOUND;
}

void proven_map_destroy(proven_map_t *map) {
    if (!map) return;
    if (map->internal.ptr && proven_alloc_is_valid(map->alloc) && map->alloc.free_fn) {
        if (map->key_type == PROVEN_KEY_TYPE_U8_OWNED) {
            for (proven_size_t i = 0; i < map->cap; ++i) {
                proven_size_t offset;
                if (PROVEN_CKD_MUL(&offset, i, map->bucket_stride)) {
                    break;
                }
                proven_map_bucket_header_t *hdr = (proven_map_bucket_header_t *)(map->internal.ptr + offset);
                if (hdr->state == BUCKET_OCCUPIED) {
                    map_release_owned_key(map, hdr);
                }
            }
        }
        map->alloc.free_fn(map->alloc.ctx, map->internal.ptr);
    }
    *map = (proven_map_t){0};
}
