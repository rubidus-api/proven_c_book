#include "proven/u8str.h"
#include "proven/align.h"
#include "proven_internal_memrange.h"
#include "../../platform/proven_sys_mem.h"

proven_result_u8str_t proven_u8str_create(proven_allocator_t alloc, proven_size_t limit) {
    proven_result_u8str_t res = {0};
    proven_size_t cap;
    if (PROVEN_CKD_ADD(&cap, limit, 1)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    
    proven_result_buf_t buf_res = proven_buf_create(alloc, cap);
    
    if (!proven_is_ok(buf_res.err)) {
        res.err = buf_res.err;
        return res;
    }
    
    buf_res.value.ptr[0] = 0;
    res.err = PROVEN_OK;
    res.value.internal = buf_res.value;
    
    return res;
}

proven_result_u8str_t proven_u8str_create_from_view(proven_allocator_t alloc, proven_u8str_view_t view) {
    proven_result_u8str_t res = proven_u8str_create(alloc, view.size);
    if (!proven_is_ok(res.err)) return res;
    
    res.err = proven_u8str_append(&res.value, view);
    if (!proven_is_ok(res.err)) {
        proven_u8str_destroy(alloc, &res.value);
    }
    return res;
}

proven_u8str_t proven_u8str_borrow(proven_byte_t *buf, proven_size_t cap) {
    proven_u8str_t s = {0};
    s.borrowed = true;
    if (!buf || cap == 0) return s;     /* empty (cap 0) borrowed string */
    buf[0] = 0;
    s.internal.ptr = buf;
    s.internal.len = 0;
    s.internal.cap = cap;
    return s;
}

proven_err_t proven_u8str_reset(proven_u8str_t *str) {
    if (!str || !str->internal.ptr) return PROVEN_ERR_INVALID_ARG;
    str->internal.len = 0;
    str->internal.ptr[0] = 0;
    return PROVEN_OK;
}

bool proven_u8str_is_valid(const proven_u8str_t *str) {
    if (!str) return false;
    if (str->internal.cap > 0) {
        if (!str->internal.ptr) return false;
        if (str->internal.len >= str->internal.cap) return false;
        if (str->internal.ptr[str->internal.len] != '\0') return false;
    } else {
        if (str->internal.len > 0) return false;
    }
    return true;
}

proven_err_t proven_u8str_append(proven_u8str_t *str, proven_u8str_view_t data) {
    if (!str || !str->internal.ptr) return PROVEN_ERR_INVALID_ARG;
    if (data.size == 0) return PROVEN_OK;
    if (!data.ptr) return PROVEN_ERR_INVALID_ARG;

    proven_size_t current_len = str->internal.len;
    proven_size_t cap = str->internal.cap;
    
    proven_size_t required;
    if (PROVEN_CKD_ADD(&required, current_len, data.size)) {
        return PROVEN_ERR_OVERFLOW;
    }
    if (PROVEN_CKD_ADD(&required, required, 1)) {
        return PROVEN_ERR_OVERFLOW;
    }
    
    if (required > cap) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    
    proven_sys_mem_move(str->internal.ptr + current_len, data.ptr, data.size);
    str->internal.len += data.size;
    str->internal.ptr[str->internal.len] = 0;
    
    return PROVEN_OK;
}

proven_result_size_t proven_u8str_append_partial(proven_u8str_t *str, proven_u8str_view_t data) {
    proven_result_size_t res = { .err = PROVEN_OK, .value = 0 };
    if (!str || !str->internal.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (data.size == 0) return res;
    if (!data.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_size_t current_len = str->internal.len;
    proven_size_t cap = str->internal.cap;
    
    if (current_len >= cap) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_size_t needed_for_term;
    if (PROVEN_CKD_ADD(&needed_for_term, current_len, 1)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    if (needed_for_term >= cap) {
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
        return res;
    }
    
    proven_size_t available = cap - current_len - 1;
    proven_size_t to_copy = data.size;
    if (to_copy > available) {
        to_copy = available;
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
    }
    
    proven_sys_mem_move(str->internal.ptr + current_len, data.ptr, to_copy);
    str->internal.len += to_copy;
    str->internal.ptr[str->internal.len] = 0;
    res.value = to_copy;
    
    return res;
}

proven_err_t proven_u8str_reserve(proven_allocator_t alloc, proven_u8str_t *str, proven_size_t new_cap) {
    if (!str) return PROVEN_ERR_INVALID_ARG;
    if (new_cap <= str->internal.cap) return PROVEN_OK;
    if (str->borrowed) return PROVEN_ERR_OUT_OF_BOUNDS;   /* cannot reallocate caller memory */
    if (!proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
    
    proven_result_mem_mut_t new_mem = alloc.realloc_fn(alloc.ctx, str->internal.ptr, str->internal.cap, new_cap, PROVEN_DEFAULT_ALIGNMENT);
    if (!proven_is_ok(new_mem.err)) return new_mem.err;

    str->internal.ptr = (proven_byte_t*)new_mem.value.ptr;
    str->internal.cap = new_cap;
    /* Re-seal the terminator. Reserving on a zero-initialized string performs
     * the first allocation, and allocators do not hand back zeroed memory, so
     * without this ptr[len] is whatever the block last held - and
     * proven_u8str_as_cstr is documented to be readable without a second
     * thought. */
    str->internal.ptr[str->internal.len] = 0;
    return PROVEN_OK;
}

proven_err_t proven_u8str_append_grow(proven_allocator_t alloc, proven_u8str_t *str, proven_u8str_view_t data) {
    if (!str) return PROVEN_ERR_INVALID_ARG;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    
    proven_size_t required;
    if (PROVEN_CKD_ADD(&required, str->internal.len, data.size)) return PROVEN_ERR_OVERFLOW;
    if (PROVEN_CKD_ADD(&required, required, 1)) return PROVEN_ERR_OVERFLOW;

    if (required > str->internal.cap) {
        if (str->borrowed) return PROVEN_ERR_OUT_OF_BOUNDS;   /* cannot reallocate caller memory */
        if (!proven_alloc_is_valid(alloc)) {
            return PROVEN_ERR_INVALID_ARG;
        }

        proven_size_t new_cap = str->internal.cap == 0 ? 16 : str->internal.cap;
        while (new_cap < required) {
            if (PROVEN_CKD_MUL(&new_cap, new_cap, 2)) {
                new_cap = required;
                break;
            }
        }

        proven_bufref_t alias_ref = proven_bufref_capture(str->internal.ptr, str->internal.cap, data.ptr, data.size);
        if (alias_ref.valid && data.size > str->internal.cap - alias_ref.offset) {
            return PROVEN_ERR_INVALID_ARG;
        }

        proven_result_mem_mut_t new_mem = alloc.realloc_fn(alloc.ctx, str->internal.ptr, str->internal.cap, new_cap, PROVEN_DEFAULT_ALIGNMENT);
        if (!proven_is_ok(new_mem.err)) {
            return new_mem.err;
        }
        
        str->internal.ptr = (proven_byte_t*)new_mem.value.ptr;
        str->internal.cap = new_cap;

        /*
         * Seal the terminator the moment the block exists.
         *
         * proven_u8str_append returns early on an empty view - correctly, there is
         * nothing to copy - so a grow that was asked to append NOTHING left a freshly
         * allocated, never-initialised block with no NUL in it. as_cstr() is documented
         * as always NUL-terminated ("guaranteed by internal structure"), and it walked
         * straight off the end of the heap block. reserve() has always sealed here; this
         * path did not. It is one store, on the growth path only.
         */
        str->internal.ptr[str->internal.len] = 0;

        if (alias_ref.valid) {
            data.ptr = (const proven_byte_t*)proven_bufref_rebase_const(alias_ref, str->internal.ptr);
        }
    }

    return proven_u8str_append(str, data);
}

proven_err_t proven_u8str_append_byte(proven_allocator_t alloc, proven_u8str_t *str, proven_u8 b) {
    proven_u8str_view_t v = { (const proven_byte_t*)&b, 1 };
    return proven_u8str_append_grow(alloc, str, v);
}

proven_result_cstr_t proven_u8str_view_to_cstr(proven_u8str_view_t view, proven_allocator_t alloc) {
    proven_result_cstr_t res = {0};
    if (!proven_alloc_is_valid(alloc)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (view.size > 0 && !view.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    
    // Check for interior NUL
    for (proven_size_t i = 0; i < view.size; ++i) {
        if (view.ptr[i] == 0) {
            res.err = PROVEN_ERR_INVALID_ARG;
            return res;
        }
    }
    
    // Allocate exact View length + 1 (for implicitly enforced '\0')
    proven_size_t cap;
    if (PROVEN_CKD_ADD(&cap, view.size, 1)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    proven_result_mem_mut_t mem = alloc.alloc_fn(alloc.ctx, cap, PROVEN_DEFAULT_ALIGNMENT);
    if (!proven_is_ok(mem.err)) {
        res.err = mem.err;
        return res;
    }
    
    // Copy the slice correctly
    if (view.size > 0) {
        proven_sys_mem_copy(mem.value.ptr, view.ptr, view.size);
    }
    mem.value.ptr[view.size] = 0; // Null-terminator sealing
    
    res.err = PROVEN_OK;
    res.value = (const char*)mem.value.ptr;
    
    return res;
}

proven_size_t proven_cstr_len(const char *s) {
    if (!s) return 0;
    const char *p = s;
    // Iterate until null terminator, limited by size_t range implicitly
    while (*p != '\0') p++;
    proven_ptrdiff_t diff = p - s;
    if (diff < 0) return 0;
    return (proven_size_t)diff;
}

int proven_u8str_view_eq(proven_u8str_view_t a, proven_u8str_view_t b) {
    if (a.size != b.size) return 0;
    if (a.size == 0) return 1;
    if (!a.ptr || !b.ptr) return 0;
    return proven_sys_mem_cmp(a.ptr, b.ptr, a.size) == 0;
}

/*
 * Substring search strategy
 * -------------------------
 * The fast path (used on ordinary text) anchors on the rarest-looking needle
 * byte and scans for it with proven_sys_mem_chr (system memchr when hosted, a
 * SWAR scan when freestanding), then verifies. That is fast because a typical
 * needle contains a byte that is rare or absent in the haystack.
 *
 * When the haystack has low entropy (a small effective alphabet: DNA, binary
 * blobs, long single-byte runs) every needle byte is common, the anchor cannot
 * be selective, and memchr+verify degrades. A cheap frequency sample detects
 * this and falls back to a self-contained linear algorithm that does not lean on
 * memchr at all (so it is robust and identical under freestanding):
 *   - needles up to 64 bytes: Shift-Or (bitap), O(n), alphabet-independent;
 *   - longer needles: Two-Way (Crochemore-Perrin), O(n), any length.
 *
 * The long-needle fallback is selectable at compile time (Two-Way is the
 * benchmark-chosen default; define PROVEN_U8STR_FIND_LONG=2 to keep the
 * memchr-adaptive scan instead). PROVEN_U8STR_FIND_FORCE forces one algorithm
 * and exists only for benchmarking/validation.
 */
#ifndef PROVEN_U8STR_FIND_LONG
#define PROVEN_U8STR_FIND_LONG 1   /* 1 = Two-Way (default), 2 = memchr-adaptive */
#endif
#ifndef PROVEN_U8STR_FIND_FORCE
#define PROVEN_U8STR_FIND_FORCE 0  /* 0 = auto; 1 = Shift-Or; 2 = Two-Way; 3 = memchr */
#endif

/* Shift-Or (bitap): needle length 1..64. Linear, no skip table, no libc.
   Returns the first match offset in h[0..n), or PROVEN_INDEX_NOT_FOUND. */
static proven_size_t proven_u8_find_shiftor(const proven_byte_t *h, proven_size_t n,
                                            const proven_byte_t *ndl, proven_size_t m) {
    proven_u64 mask[256];
    proven_u64 all_ones = ~(proven_u64)0;
    proven_u64 state = all_ones;
    proven_u64 match_bit = (proven_u64)1 << (m - 1u);
    proven_size_t j;
    for (j = 0; j < 256u; ++j) mask[j] = all_ones;
    for (j = 0; j < m; ++j) mask[ndl[j]] &= ~((proven_u64)1 << j);
    for (j = 0; j < n; ++j) {
        state = (state << 1) | mask[h[j]];
        if ((state & match_bit) == 0u) {
            return j + 1u - m;
        }
    }
    return PROVEN_INDEX_NOT_FOUND;
}

/* Two-Way (Crochemore-Perrin), ported from the musl libc memmem. O(n) for any
   needle length, no libc, freestanding-safe. The (size_t)-1 index wraparound is
   intentional (n[ip+k] with ip == -1 reads n[k-1]). */
#define PROVEN_FIND_BITOP(a, b, op) \
    ((a)[(proven_size_t)(b) / (8u * sizeof *(a))] op (proven_size_t)1 << ((proven_size_t)(b) % (8u * sizeof *(a))))
static proven_size_t proven_u8_find_twoway(const proven_byte_t *h0, proven_size_t hn,
                                           const proven_byte_t *n, proven_size_t l) {
    const proven_byte_t *h = h0;
    const proven_byte_t *hend = h0 + hn;
    proven_size_t i, ip, jp, k, p, ms, p0, mem, mem0;
    proven_size_t byteset[32 / sizeof(proven_size_t)] = { 0 };
    proven_size_t shift[256];

    for (i = 0; i < l; ++i) {
        PROVEN_FIND_BITOP(byteset, n[i], |=);
        shift[n[i]] = i + 1u;
    }
    /* maximal suffix, ordinary ordering */
    ip = (proven_size_t)-1; jp = 0; k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) { if (k == p) { jp += p; k = 1; } else ++k; }
        else if (n[ip + k] > n[jp + k]) { jp += k; k = 1; p = jp - ip; }
        else { ip = jp++; k = 1; p = 1; }
    }
    ms = ip; p0 = p;
    /* maximal suffix, reversed ordering */
    ip = (proven_size_t)-1; jp = 0; k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) { if (k == p) { jp += p; k = 1; } else ++k; }
        else if (n[ip + k] < n[jp + k]) { jp += k; k = 1; p = jp - ip; }
        else { ip = jp++; k = 1; p = 1; }
    }
    if (ip + 1u > ms + 1u) ms = ip; else p = p0;

    if (proven_sys_mem_cmp(n, n + p, ms + 1u) != 0) {
        mem0 = 0;
        p = ((ms > l - ms - 1u) ? ms : (l - ms - 1u)) + 1u;
    } else {
        mem0 = l - p;
    }
    mem = 0;

    for (;;) {
        if ((proven_size_t)(hend - h) < l) return PROVEN_INDEX_NOT_FOUND;
        if (PROVEN_FIND_BITOP(byteset, h[l - 1u], &)) {
            k = l - shift[h[l - 1u]];
            if (k) {
                if (k < mem) k = mem;
                h += k; mem = 0; continue;
            }
        } else {
            h += l; mem = 0; continue;
        }
        for (k = (ms + 1u > mem ? ms + 1u : mem); k < l && n[k] == h[k]; ++k) { }
        if (k < l) { h += k - ms; mem = 0; continue; }
        for (k = ms + 1u; k > mem && n[k - 1u] == h[k - 1u]; --k) { }
        if (k <= mem) return (proven_size_t)(h - h0);
        h += p; mem = mem0;
    }
}

proven_size_t proven_u8str_view_find(proven_u8str_view_t haystack, proven_size_t start_offset, proven_u8str_view_t needle) {
    if (start_offset > haystack.size) return PROVEN_INDEX_NOT_FOUND;
    if (needle.size == 0) return start_offset; // Empty needle always matches at offset
    if (haystack.size > 0 && !haystack.ptr) return PROVEN_INDEX_NOT_FOUND;
    if (needle.size > 0 && !needle.ptr) return PROVEN_INDEX_NOT_FOUND;
    if (needle.size > haystack.size - start_offset) return PROVEN_INDEX_NOT_FOUND;

    const proven_byte_t *base = haystack.ptr;
    proven_size_t span = haystack.size - start_offset;

    if (needle.size == 1u) {
        const void *hit = proven_sys_mem_chr(base + start_offset, needle.ptr[0], span);
        return hit ? (proven_size_t)((const proven_byte_t *)hit - base) : PROVEN_INDEX_NOT_FOUND;
    }

#if PROVEN_U8STR_FIND_FORCE == 1
    {
        proven_size_t r = proven_u8_find_shiftor(base + start_offset, span, needle.ptr, needle.size);
        return (r == PROVEN_INDEX_NOT_FOUND) ? r : start_offset + r;
    }
#elif PROVEN_U8STR_FIND_FORCE == 2
    {
        proven_size_t r = proven_u8_find_twoway(base + start_offset, span, needle.ptr, needle.size);
        return (r == PROVEN_INDEX_NOT_FOUND) ? r : start_offset + r;
    }
#else
    /* Pick the anchor: the rarest-looking needle byte, from a spread-out sample.
       Also learn whether even that rarest byte is common (low-entropy haystack). */
    proven_size_t anchor = needle.size - 1u;
    int low_entropy = 0;
    if (span >= 16u) {
        unsigned short cnt[256];
        proven_sys_mem_zero(cnt, sizeof cnt);
        proven_size_t cap = span < 256u ? span : 256u;
        proven_size_t step = span / cap;
        if (step == 0u) step = 1u;
        step |= 1u; /* odd stride: avoid aliasing a periodic haystack (e.g. strict
                       "abab…") to a single phase, which would hide a common byte */
        proven_size_t taken = 0;
        for (proven_size_t pos = start_offset; pos < haystack.size && taken < 256u; pos += step) {
            cnt[base[pos]]++;
            ++taken;
        }
        unsigned best = 0xffffffffu;
        for (proven_size_t i = 0; i < needle.size; ++i) {
            unsigned ch = (unsigned)needle.ptr[i];
            if ((unsigned)cnt[ch] <= best) { best = (unsigned)cnt[ch]; anchor = i; }
        }
        /* rarest needle byte still in > ~1/8 of the sample => memchr+verify will
           thrash; use the alphabet-independent linear fallback instead. */
        low_entropy = (best * 8u > (unsigned)taken);
    }

#if PROVEN_U8STR_FIND_FORCE != 3
    if (low_entropy) {
        if (needle.size <= 64u) {
            proven_size_t r = proven_u8_find_shiftor(base + start_offset, span, needle.ptr, needle.size);
            return (r == PROVEN_INDEX_NOT_FOUND) ? r : start_offset + r;
        }
#if PROVEN_U8STR_FIND_LONG == 1
        {
            proven_size_t r = proven_u8_find_twoway(base + start_offset, span, needle.ptr, needle.size);
            return (r == PROVEN_INDEX_NOT_FOUND) ? r : start_offset + r;
        }
#endif
    }
#endif

    proven_byte_t anchor_c = needle.ptr[anchor];
    /* Scan the anchor only where the whole needle still fits. */
    const proven_byte_t *scan = base + start_offset + anchor;
    proven_size_t scan_n = (haystack.size - needle.size) - start_offset + 1u;

    while (scan_n > 0u) {
        const void *hitv = proven_sys_mem_chr(scan, anchor_c, scan_n);
        if (!hitv) break;
        const proven_byte_t *hit = (const proven_byte_t *)hitv;
        const proven_byte_t *match = hit - anchor;
        if (proven_sys_mem_cmp(match, needle.ptr, anchor) == 0 &&
            proven_sys_mem_cmp(hit + 1, needle.ptr + anchor + 1u, needle.size - anchor - 1u) == 0) {
            return (proven_size_t)(match - base);
        }
        proven_size_t consumed = (proven_size_t)(hit - scan) + 1u;
        if (consumed >= scan_n) break;
        scan = hit + 1;
        scan_n -= consumed;
    }
    return PROVEN_INDEX_NOT_FOUND;
#endif
}

int proven_u8str_view_starts_with(proven_u8str_view_t str, proven_u8str_view_t prefix) {
    if (prefix.size == 0) return 1;
    if (str.size < prefix.size) return 0;
    if (!str.ptr || !prefix.ptr) return 0;
    return proven_sys_mem_cmp(str.ptr, prefix.ptr, prefix.size) == 0;
}

int proven_u8str_view_ends_with(proven_u8str_view_t str, proven_u8str_view_t suffix) {
    if (suffix.size == 0) return 1;
    if (str.size < suffix.size) return 0;
    if (!str.ptr || !suffix.ptr) return 0;
    proven_size_t offset = str.size - suffix.size;
    return proven_sys_mem_cmp(str.ptr + offset, suffix.ptr, suffix.size) == 0;
}

proven_u8str_view_t proven_u8str_view_slice(proven_u8str_view_t str, proven_size_t index, proven_size_t len) {
    if (str.size > 0 && !str.ptr) {
        return (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 };
    }
    if (len == 0) {
        if (index > str.size) {
            return (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 };
        }
        return (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 };
    }
    if (index >= str.size) {
        return (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 };
    }
    // Clamp length
    proven_size_t actual_len = len;
    proven_size_t end_idx;
    if (PROVEN_CKD_ADD(&end_idx, index, len) || end_idx > str.size) {
        actual_len = str.size - index;
    }
    return (proven_u8str_view_t){ .ptr = str.ptr + index, .size = actual_len };
}

proven_err_t proven_u8str_replace_at(proven_u8str_t *str, proven_size_t index, proven_size_t old_len, proven_u8str_view_t data) {
    if (!str || !str->internal.ptr) return PROVEN_ERR_INVALID_ARG;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    if (index > str->internal.len) return PROVEN_ERR_OUT_OF_BOUNDS; // index must be <= len, allow append at the exact end.
    
    // Clamp old_len to the actual available size backwards from the string end
    proven_size_t actual_old = old_len;
    proven_size_t total_end;
    if (PROVEN_CKD_ADD(&total_end, index, old_len) || total_end > str->internal.len) {
        actual_old = str->internal.len - index;
    }
    
    // Check total capacity relative to what we can do
    proven_size_t new_total_len;
    if (PROVEN_CKD_SUB(&new_total_len, str->internal.len, actual_old)) {
        return PROVEN_ERR_OVERFLOW;
    }
    if (PROVEN_CKD_ADD(&new_total_len, new_total_len, data.size)) {
        return PROVEN_ERR_OVERFLOW;
    }

    proven_size_t required;
    if (PROVEN_CKD_ADD(&required, new_total_len, 1)) {
        return PROVEN_ERR_OVERFLOW;
    }
    if (required > str->internal.cap) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    
    proven_byte_t *base = str->internal.ptr;
    
    // Check for logical corruption: replacement source aliases this string buffer 
    // AND the size changes requiring tail shifting.
    proven_bufref_t alias_ref = proven_bufref_capture(base, str->internal.cap, data.ptr, data.size);
    if (alias_ref.valid && data.size != actual_old && index + actual_old < str->internal.len) {
        return PROVEN_ERR_INVALID_ARG;
    }
    
    // Do we need to move the tail?
    if (data.size != actual_old && index + actual_old < str->internal.len) {
        proven_size_t tail_len = str->internal.len - (index + actual_old);
        proven_sys_mem_move(base + index + data.size, base + index + actual_old, tail_len);
    }
    
    // Copy new data
    if (data.size > 0) {
        proven_sys_mem_move(base + index, data.ptr, data.size);
    }
    
    str->internal.len = new_total_len;
    base[new_total_len] = 0; // Null-terminator sealing guarantee
    
    return PROVEN_OK;
}

proven_err_t proven_u8str_insert(proven_u8str_t *str, proven_size_t index, proven_u8str_view_t data) {
    return proven_u8str_replace_at(str, index, 0, data);
}

proven_err_t proven_u8str_replace_at_grow(proven_allocator_t alloc, proven_u8str_t *str, proven_size_t index, proven_size_t old_len, proven_u8str_view_t data) {
    if (!str || !str->internal.ptr) return PROVEN_ERR_INVALID_ARG;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    if (index > str->internal.len) return PROVEN_ERR_OUT_OF_BOUNDS;

    /* Clamp old_len the same way replace_at does, then size the grow to the
       exact post-edit byte length (plus the NUL seal). */
    proven_size_t actual_old = old_len;
    proven_size_t total_end;
    if (PROVEN_CKD_ADD(&total_end, index, old_len) || total_end > str->internal.len) {
        actual_old = str->internal.len - index;
    }

    proven_size_t new_total_len;
    if (PROVEN_CKD_SUB(&new_total_len, str->internal.len, actual_old)) return PROVEN_ERR_OVERFLOW;
    if (PROVEN_CKD_ADD(&new_total_len, new_total_len, data.size)) return PROVEN_ERR_OVERFLOW;
    proven_size_t required;
    if (PROVEN_CKD_ADD(&required, new_total_len, 1)) return PROVEN_ERR_OVERFLOW;

    if (required > str->internal.cap) {
        if (str->borrowed) return PROVEN_ERR_OUT_OF_BOUNDS;   /* cannot reallocate caller memory */
        if (!proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;

        proven_size_t new_cap = str->internal.cap == 0 ? 16 : str->internal.cap;
        while (new_cap < required) {
            if (PROVEN_CKD_MUL(&new_cap, new_cap, 2)) {
                new_cap = required;
                break;
            }
        }

        /* If data points into this string's buffer, rebase it across the realloc
           so replace_at sees a consistent pointer. */
        proven_bufref_t alias_ref = proven_bufref_capture(str->internal.ptr, str->internal.cap, data.ptr, data.size);
        proven_result_mem_mut_t new_mem = alloc.realloc_fn(alloc.ctx, str->internal.ptr, str->internal.cap, new_cap, PROVEN_DEFAULT_ALIGNMENT);
        if (!proven_is_ok(new_mem.err)) return new_mem.err;
        str->internal.ptr = (proven_byte_t*)new_mem.value.ptr;
        str->internal.cap = new_cap;
        if (alias_ref.valid) {
            data.ptr = (const proven_byte_t*)proven_bufref_rebase_const(alias_ref, str->internal.ptr);
        }
    }

    return proven_u8str_replace_at(str, index, old_len, data);
}

proven_err_t proven_u8str_insert_grow(proven_allocator_t alloc, proven_u8str_t *str, proven_size_t index, proven_u8str_view_t data) {
    return proven_u8str_replace_at_grow(alloc, str, index, 0, data);
}

proven_err_t proven_u8str_remove(proven_u8str_t *str, proven_size_t index, proven_size_t len) {
    return proven_u8str_replace_at(str, index, len, (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 });
}

proven_err_t proven_u8str_replace_first(proven_u8str_t *str, proven_size_t start_offset, proven_u8str_view_t target, proven_u8str_view_t replacement) {
    if (target.size == 0) return PROVEN_ERR_INVALID_ARG;
    proven_size_t idx = proven_u8str_view_find(proven_u8str_as_view(str), start_offset, target);
    if (idx == PROVEN_INDEX_NOT_FOUND) return PROVEN_OK; // Or PROVEN_INDEX_NOT_FOUND error? PROVEN_OK matches meaning "done/nothing to do"
    
    return proven_u8str_replace_at(str, idx, target.size, replacement);
}

void proven_u8str_destroy(proven_allocator_t alloc, proven_u8str_t *str) {
    if (!str) return;
    if (str->borrowed) {                 /* caller owns the memory: free nothing */
        str->internal = (proven_buf_t){0};
        return;
    }
    proven_buf_destroy(alloc, &str->internal);
}
