#include "proven/array.h"
#include "proven_internal_memrange.h"
#include "../../platform/proven_sys_mem.h"

proven_result_array_t proven_array_create(proven_allocator_t alloc, proven_size_t init_cap, proven_size_t elem_size, proven_size_t align) {
    proven_result_array_t res = {0};
    
    if (elem_size == 0 || !proven_alloc_is_valid(alloc) || !proven_is_pow2(align)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_byte_t *data = (void*)0;
    
    // Abstracted Pre-allocation
    if (init_cap > 0) {
        proven_size_t bytes_needed;
        if (PROVEN_CKD_MUL(&bytes_needed, init_cap, elem_size)) {
            res.err = PROVEN_ERR_NOMEM;
            return res;
        }
        proven_result_mem_mut_t alloc_res = alloc.alloc_fn(alloc.ctx, bytes_needed, align);
        if (!PROVEN_IS_OK(alloc_res.err)) {
            res.err = alloc_res.err;
            return res;
        }
        data = alloc_res.value.ptr;
    }

    proven_array_t arr = {
        .alloc = alloc,
        .data = data,
        .len = 0,
        .cap = init_cap,
        .elem_size = elem_size,
        .align = align
    };

    res.err = PROVEN_OK;
    res.value = arr;
    return res;
}

bool proven_array_is_valid(const proven_array_t *arr) {
    if (!arr) return false;
    if (arr->elem_size == 0 || !proven_is_pow2(arr->align)) return false;
    if (arr->len > arr->cap) return false;
    if (arr->cap > 0 && !arr->data) return false;
    if (arr->len > 0 && !arr->data) return false;
    
    proven_size_t bytes_needed;
    if (PROVEN_CKD_MUL(&bytes_needed, arr->cap, arr->elem_size)) return false;
    if (!proven_alloc_is_valid(arr->alloc)) return false;
    
    return true;
}

proven_err_t proven_array_reserve(proven_array_t *arr, proven_size_t new_cap) {
    if (!proven_array_is_valid(arr)) return PROVEN_ERR_INVALID_ARG;
    if (new_cap <= arr->cap) return PROVEN_OK;
    
    proven_size_t new_bytes;
    if (PROVEN_CKD_MUL(&new_bytes, new_cap, arr->elem_size)) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    
    proven_size_t old_bytes;
    if (PROVEN_CKD_MUL(&old_bytes, arr->cap, arr->elem_size)) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }

    proven_result_mem_mut_t realloc_res = arr->alloc.realloc_fn(arr->alloc.ctx, arr->data, old_bytes, new_bytes, arr->align);
    if (!PROVEN_IS_OK(realloc_res.err)) {
        return realloc_res.err;
    }

    arr->data = realloc_res.value.ptr;
    arr->cap = new_cap;
    return PROVEN_OK;
}

proven_err_t proven_array_push(proven_array_t *arr, const void *element) {
    if (!proven_array_is_valid(arr) || !element) return PROVEN_ERR_INVALID_ARG;

    // Bounds limit reached, engage dynamic expansion!
    if (arr->len >= arr->cap) {
        proven_size_t new_cap;
        if (arr->cap == 0) {
            new_cap = 8;
        } else if (PROVEN_CKD_MUL(&new_cap, arr->cap, 2)) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        
        proven_size_t new_bytes;
        if (PROVEN_CKD_MUL(&new_bytes, new_cap, arr->elem_size)) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        
        proven_size_t old_bytes;
        if (PROVEN_CKD_MUL(&old_bytes, arr->cap, arr->elem_size)) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }

        proven_bufref_t alias_ref = proven_bufref_capture(arr->data, old_bytes, element, arr->elem_size);
        if (alias_ref.valid && arr->elem_size > old_bytes - alias_ref.offset) {
            return PROVEN_ERR_INVALID_ARG;
        }

        proven_result_mem_mut_t realloc_res = arr->alloc.realloc_fn(arr->alloc.ctx, arr->data, old_bytes, new_bytes, arr->align);
        
        if (!PROVEN_IS_OK(realloc_res.err)) {
            return realloc_res.err;
        }

        // Commit pointer override atomically
        arr->data = realloc_res.value.ptr;
        arr->cap = new_cap;

        if (alias_ref.valid) {
            element = proven_bufref_rebase_const(alias_ref, arr->data);
        }
    }

    // Embed bytes onto offset index matching array state
    proven_size_t offset;
    if (PROVEN_CKD_MUL(&offset, arr->len, arr->elem_size)) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    proven_byte_t *dst = arr->data + offset;
    proven_sys_mem_copy(dst, element, arr->elem_size);
    
    arr->len++;
    return PROVEN_OK;
}

proven_err_t proven_array_pop(proven_array_t *arr, void *out_element) {
    if (!proven_array_is_valid(arr)) return PROVEN_ERR_INVALID_ARG;
    if (arr->len == 0) return PROVEN_ERR_OUT_OF_BOUNDS;

    arr->len--;
    if (out_element) {
        proven_size_t offset;
        if (PROVEN_CKD_MUL(&offset, arr->len, arr->elem_size)) {
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        proven_sys_mem_copy(out_element, arr->data + offset, arr->elem_size);
    }
    return PROVEN_OK;
}

void* proven_array_get_mut(proven_array_t *arr, proven_size_t index) {
    if (!proven_array_is_valid(arr) || index >= arr->len) return (void*)0;
    
    proven_size_t offset;
    if (PROVEN_CKD_MUL(&offset, index, arr->elem_size)) {
        return (void*)0;
    }
    return arr->data + offset;
}

const void* proven_array_get(const proven_array_t *arr, proven_size_t index) {
    if (!proven_array_is_valid(arr) || index >= arr->len) return (const void*)0;
    
    proven_size_t offset;
    if (PROVEN_CKD_MUL(&offset, index, arr->elem_size)) {
        return (const void*)0;
    }
    return arr->data + offset;
}

/* ---- RFC-0009 X-002: the operations callers wrote by hand on top of get_mut and len ---- */

/* Room for `extra` more elements: doubling like push, or straight to the need if doubling is
 * short of it. `src`/`src_bytes` is a range the caller is about to copy from; if it lies inside
 * the array it is rebased onto the new storage, and one that only partly overlaps the array is
 * refused - after a reallocation half of it would be freed memory. */
static proven_err_t array_make_room(proven_array_t *arr, proven_size_t extra,
                                    const void **src, proven_size_t src_bytes) {
    proven_size_t need;
    if (PROVEN_CKD_ADD(&need, arr->len, extra)) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_size_t old_bytes;
    if (PROVEN_CKD_MUL(&old_bytes, arr->cap, arr->elem_size)) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_bufref_t ref = { .valid = false, .offset = 0, .size = 0 };
    if (src && *src && src_bytes > 0) {
        ref = proven_bufref_capture(arr->data, old_bytes, *src, src_bytes);
        if (!ref.valid && proven_range_overlaps(arr->data, old_bytes, *src, src_bytes)) return PROVEN_ERR_INVALID_ARG;
    }
    if (need <= arr->cap) return PROVEN_OK;

    proven_size_t new_cap = arr->cap == 0 ? 8 : arr->cap;
    while (new_cap < need) {
        if (PROVEN_CKD_MUL(&new_cap, new_cap, 2)) { new_cap = need; break; }
    }
    proven_size_t new_bytes;
    if (PROVEN_CKD_MUL(&new_bytes, new_cap, arr->elem_size)) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_result_mem_mut_t r = arr->alloc.realloc_fn(arr->alloc.ctx, arr->data, old_bytes, new_bytes, arr->align);
    if (!PROVEN_IS_OK(r.err)) return r.err;
    arr->data = r.value.ptr;
    arr->cap = new_cap;
    if (ref.valid) *src = proven_bufref_rebase_const(ref, arr->data);
    return PROVEN_OK;
}

void proven_array_clear(proven_array_t *arr) {
    if (proven_array_is_valid(arr)) arr->len = 0;
}

proven_err_t proven_array_truncate(proven_array_t *arr, proven_size_t new_len) {
    if (!proven_array_is_valid(arr)) return PROVEN_ERR_INVALID_ARG;
    if (new_len > arr->len) return PROVEN_ERR_OUT_OF_BOUNDS;
    arr->len = new_len;
    return PROVEN_OK;
}

proven_err_t proven_array_insert(proven_array_t *arr, proven_size_t index, const void *element) {
    if (!proven_array_is_valid(arr) || !element) return PROVEN_ERR_INVALID_ARG;
    if (index > arr->len) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_err_t e = array_make_room(arr, 1, &element, arr->elem_size);
    if (!proven_is_ok(e)) return e;
    proven_byte_t *at = arr->data + index * arr->elem_size;            /* index <= len < cap: no overflow */
    proven_size_t tail = (arr->len - index) * arr->elem_size;
    /* An element that lives in the part about to move moves with it. */
    const proven_byte_t *el = element;
    proven_uintptr_t ep = (proven_uintptr_t)el, ap = (proven_uintptr_t)at;
    if (ep >= ap && ep < ap + tail) el = at + (ep - ap) + arr->elem_size;
    proven_sys_mem_move(at + arr->elem_size, at, tail);
    proven_sys_mem_copy(at, el, arr->elem_size);
    arr->len++;
    return PROVEN_OK;
}

proven_err_t proven_array_remove_at(proven_array_t *arr, proven_size_t index, void *out_element) {
    if (!proven_array_is_valid(arr)) return PROVEN_ERR_INVALID_ARG;
    if (index >= arr->len) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_byte_t *at = arr->data + index * arr->elem_size;
    if (out_element) proven_sys_mem_copy(out_element, at, arr->elem_size);
    proven_sys_mem_move(at, at + arr->elem_size, (arr->len - index - 1) * arr->elem_size);
    arr->len--;
    return PROVEN_OK;
}

proven_err_t proven_array_swap_remove(proven_array_t *arr, proven_size_t index, void *out_element) {
    if (!proven_array_is_valid(arr)) return PROVEN_ERR_INVALID_ARG;
    if (index >= arr->len) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_byte_t *at = arr->data + index * arr->elem_size;
    if (out_element) proven_sys_mem_copy(out_element, at, arr->elem_size);
    arr->len--;
    if (index != arr->len) proven_sys_mem_copy(at, arr->data + arr->len * arr->elem_size, arr->elem_size);
    return PROVEN_OK;
}

proven_err_t proven_array_extend(proven_array_t *arr, const void *elements, proven_size_t count) {
    if (!proven_array_is_valid(arr) || (count > 0 && !elements)) return PROVEN_ERR_INVALID_ARG;
    if (count == 0) return PROVEN_OK;
    proven_size_t bytes;
    if (PROVEN_CKD_MUL(&bytes, count, arr->elem_size)) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_err_t e = array_make_room(arr, count, &elements, bytes);
    if (!proven_is_ok(e)) return e;
    /* move, not copy: the source may be the array's own elements */
    proven_sys_mem_move(arr->data + arr->len * arr->elem_size, elements, bytes);
    arr->len += count;
    return PROVEN_OK;
}

void proven_array_destroy(proven_array_t *arr) {
    if (!arr) return;
    if (arr->data && proven_alloc_is_valid(arr->alloc) && arr->alloc.free_fn) {
        arr->alloc.free_fn(arr->alloc.ctx, arr->data); // Cleaned by trait mapping safely
    }
    arr->data = (void*)0;
    arr->len = 0;
    arr->cap = 0;
    arr->elem_size = 0;
    arr->align = 0;
}
