#include "proven/alloc_check.h"
#include "proven/panic.h"

/*
 * The record is a plain array searched linearly: this is a checking tool, and a lookup that is
 * obviously right is worth more here than one that is fast. Removal swaps the last entry into the
 * hole, so the array stays dense.
 *
 * Nothing refused is ever passed to the inner allocator: handing it a foreign or freed pointer is
 * the corruption this exists to stop, and a panic handler that returns must not cause it anyway.
 */

static proven_size_t find_entry(const proven_alloc_check_t *st, const void *ptr) {
    for (proven_size_t i = 0; i < st->live; ++i) {
        if (st->entries[i].ptr == ptr) return i;
    }
    return PROVEN_SIZE_MAX;
}

static void refuse(proven_alloc_check_t *st, const char *msg) {
    st->faults++;
    proven_panic(msg);
}

static void forget(proven_alloc_check_t *st, proven_size_t i) {
    st->live_bytes -= st->entries[i].size;
    st->entries[i] = st->entries[st->live - 1];
    st->live--;
}

static bool remember(proven_alloc_check_t *st, void *ptr, proven_size_t size, proven_size_t align) {
    if (st->live == st->cap) {
        refuse(st, "alloc_check: the record is full - give proven_alloc_check_wrap a larger table");
        return false;
    }
    st->entries[st->live++] = (proven_alloc_check_entry_t){ ptr, size, align };
    st->live_bytes += size;
    if (st->live > st->peak_live) st->peak_live = st->live;
    return true;
}

static proven_result_mem_mut_t check_alloc(void *ctx, proven_size_t size, proven_size_t align) {
    proven_alloc_check_t *st = ctx;
    proven_result_mem_mut_t r = st->inner.alloc_fn(st->inner.ctx, size, align);
    if (!proven_is_ok(r.err) || !r.value.ptr) return r;
    if (!remember(st, r.value.ptr, size, align)) {
        st->inner.free_fn(st->inner.ctx, r.value.ptr);   /* ours to give back: it was never handed out */
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_NOMEM };
    }
    return r;
}

static void check_free(void *ctx, void *ptr) {
    proven_alloc_check_t *st = ctx;
    if (!ptr) return;
    proven_size_t i = find_entry(st, ptr);
    if (i == PROVEN_SIZE_MAX) {
        refuse(st, "alloc_check: free of a block this allocator does not own - another allocator's block, or a double free");
        return;
    }
    forget(st, i);
    st->inner.free_fn(st->inner.ctx, ptr);
}

static proven_result_mem_mut_t check_realloc(void *ctx, void *old_ptr, proven_size_t old_size,
                                             proven_size_t new_size, proven_size_t align) {
    proven_alloc_check_t *st = ctx;
    if (!old_ptr) {
        if (new_size == 0) return (proven_result_mem_mut_t){ .err = PROVEN_OK };
        return check_alloc(ctx, new_size, align);
    }
    proven_size_t i = find_entry(st, old_ptr);
    if (i == PROVEN_SIZE_MAX) {
        refuse(st, "alloc_check: realloc of a block this allocator does not own - another allocator's block, or one already freed");
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    if (st->entries[i].size != old_size || st->entries[i].align != align) {
        /* allocator.h: old_size and align must be the ones the block was allocated with. An
         * allocator that trusts a wrong old_size copies past the block or loses its tail. */
        refuse(st, "alloc_check: realloc with an old size or alignment the block was not allocated with");
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_INVALID_ARG };
    }

    proven_result_mem_mut_t r = st->inner.realloc_fn(st->inner.ctx, old_ptr, old_size, new_size, align);
    if (!proven_is_ok(r.err)) return r;              /* the old block is untouched and still ours */
    if (new_size == 0) {                             /* the contract: new_size 0 frees */
        forget(st, i);
        return r;
    }
    st->live_bytes = st->live_bytes - st->entries[i].size + new_size;
    st->entries[i].ptr = r.value.ptr;
    st->entries[i].size = new_size;
    return r;
}

proven_allocator_t proven_alloc_check_wrap(proven_alloc_check_t *st, proven_allocator_t inner,
                                           proven_alloc_check_entry_t *entries, proven_size_t cap) {
    if (!st || !proven_alloc_is_valid(inner) || (cap > 0 && !entries)) return (proven_allocator_t){0};
    *st = (proven_alloc_check_t){ .inner = inner, .entries = entries, .cap = cap };
    return (proven_allocator_t){ .ctx = st, .alloc_fn = check_alloc, .realloc_fn = check_realloc, .free_fn = check_free };
}

bool proven_alloc_check_owns(const proven_alloc_check_t *st, const void *ptr) {
    return st && ptr && find_entry(st, ptr) != PROVEN_SIZE_MAX;
}

proven_size_t proven_alloc_check_live(const proven_alloc_check_t *st) {
    return st ? st->live : 0;
}
