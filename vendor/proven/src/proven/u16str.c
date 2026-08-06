#include "proven/u16str.h"

#ifndef PROVEN_NO_U16STR

#include "proven_internal_memrange.h"
#include "../../platform/proven_sys_mem.h"

proven_result_u16str_t proven_u16str_create(proven_allocator_t alloc, proven_size_t unit_limit) {
    proven_result_u16str_t res = {0};
    proven_size_t byte_cap;
    
    // cap + 1 for null terminator
    if (PROVEN_CKD_ADD(&byte_cap, unit_limit, 1)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    if (PROVEN_CKD_MUL(&byte_cap, byte_cap, sizeof(proven_u16))) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    proven_result_buf_t buf_res = proven_buf_create(alloc, byte_cap);
    res.err = buf_res.err;
    if (proven_is_ok(res.err)) {
        res.value.internal = buf_res.value;
        // Ensure null termination on empty create
        if (res.value.internal.ptr) {
            ((proven_u16*)res.value.internal.ptr)[0] = 0;
        }
    }
    return res;
}

void proven_u16str_destroy(proven_allocator_t alloc, proven_u16str_t *str) {
    if (str) {
        proven_buf_destroy(alloc, &str->internal);
    }
}

proven_result_u16str_t proven_u16str_create_from_view(proven_allocator_t alloc, proven_u16str_view_t view) {
    proven_result_u16str_t res = proven_u16str_create(alloc, view.size);
    if (!proven_is_ok(res.err)) return res;
    
    res.err = proven_u16str_append(&res.value, view);
    if (!proven_is_ok(res.err)) {
        proven_u16str_destroy(alloc, &res.value);
    }
    return res;
}

proven_err_t proven_u16str_append(proven_u16str_t *str, proven_u16str_view_t data) {
    if (!str || !str->internal.ptr) return PROVEN_ERR_INVALID_ARG;
    if (data.size == 0) return PROVEN_OK;
    if (!data.ptr) return PROVEN_ERR_INVALID_ARG;

    proven_size_t current_units = str->internal.len / sizeof(proven_u16);
    proven_size_t cap_units = str->internal.cap / sizeof(proven_u16);
    
    proven_size_t required_units;
    if (PROVEN_CKD_ADD(&required_units, current_units, data.size)) return PROVEN_ERR_OVERFLOW;
    if (PROVEN_CKD_ADD(&required_units, required_units, 1)) return PROVEN_ERR_OVERFLOW;
    
    if (required_units > cap_units) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }
    
    proven_u16 *ptr = (proven_u16*)str->internal.ptr;
    proven_size_t bytes;
    if (PROVEN_CKD_MUL(&bytes, data.size, sizeof(proven_u16))) return PROVEN_ERR_OVERFLOW;
    
    proven_sys_mem_move(ptr + current_units, data.ptr, bytes);
    str->internal.len += bytes;
    ptr[str->internal.len / sizeof(proven_u16)] = 0;
    
    return PROVEN_OK;
}

proven_result_size_t proven_u16str_append_partial(proven_u16str_t *str, proven_u16str_view_t data) {
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

    proven_size_t current_units = str->internal.len / sizeof(proven_u16);
    proven_size_t cap_units = str->internal.cap / sizeof(proven_u16);
    
    if (current_units >= cap_units) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_size_t needed_for_term;
    if (PROVEN_CKD_ADD(&needed_for_term, current_units, 1)) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    if (needed_for_term >= cap_units) {
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
        return res;
    }
    
    proven_size_t available = cap_units - current_units - 1; // Explicitly 1 for NUL
    if (available == 0) {
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
        return res;
    }
    
    proven_size_t to_copy = data.size;
    if (to_copy > available) {
        to_copy = available;
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
    }
    
    proven_u16 *ptr = (proven_u16*)str->internal.ptr;
    proven_size_t bytes;
    if (PROVEN_CKD_MUL(&bytes, to_copy, sizeof(proven_u16))) {
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }
    
    proven_sys_mem_move(ptr + current_units, data.ptr, bytes);
    str->internal.len += bytes;
    ptr[str->internal.len / sizeof(proven_u16)] = 0;
    res.value = to_copy;
    
    return res;
}

proven_err_t proven_u16str_append_grow(proven_allocator_t alloc, proven_u16str_t *str, proven_u16str_view_t data) {
    if (!str) return PROVEN_ERR_INVALID_ARG;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    
    proven_size_t current_units = str->internal.len / sizeof(proven_u16);
    proven_size_t required_units;
    if (PROVEN_CKD_ADD(&required_units, current_units, data.size)) return PROVEN_ERR_OVERFLOW;
    if (PROVEN_CKD_ADD(&required_units, required_units, 1)) return PROVEN_ERR_OVERFLOW;

    proven_size_t required_bytes;
    if (PROVEN_CKD_MUL(&required_bytes, required_units, sizeof(proven_u16))) return PROVEN_ERR_OVERFLOW;

    if (required_bytes > str->internal.cap) {
        if (!proven_alloc_is_valid(alloc)) {
            return PROVEN_ERR_INVALID_ARG;
        }

        proven_size_t new_cap = str->internal.cap == 0 ? (16 * sizeof(proven_u16)) : str->internal.cap;
        while (new_cap < required_bytes) {
            if (PROVEN_CKD_MUL(&new_cap, new_cap, 2)) {
                new_cap = required_bytes;
                break;
            }
        }
        
        proven_size_t data_bytes = 0;
        if (PROVEN_CKD_MUL(&data_bytes, data.size, sizeof(proven_u16))) {
            return PROVEN_ERR_OVERFLOW;
        }
        proven_bufref_t alias_ref = proven_bufref_capture(str->internal.ptr, str->internal.cap, data.ptr, data_bytes);

        proven_result_mem_mut_t new_mem = alloc.realloc_fn(alloc.ctx, str->internal.ptr, str->internal.cap, new_cap, PROVEN_DEFAULT_ALIGNMENT);
        if (!proven_is_ok(new_mem.err)) {
            return new_mem.err;
        }
        
        str->internal.ptr = (proven_byte_t*)new_mem.value.ptr;
        str->internal.cap = new_cap;

        /* Same seal, same reason as u8str: an append of nothing must still leave a
         * terminated string, because as_cstr promises one whatever happened. internal.len
         * is a BYTE count, so the terminator's u16 index is len/sizeof(proven_u16) - not len,
         * which would write at twice the offset (harmless in the doubling slack today, an
         * out-of-bounds write the moment growth allocates exactly what is required). */
        ((proven_u16 *)(void *)str->internal.ptr)[str->internal.len / sizeof(proven_u16)] = 0;

        if (alias_ref.valid) {
            data.ptr = (const proven_u16*)proven_bufref_rebase_const(alias_ref, str->internal.ptr);
        }
    }

    return proven_u16str_append(str, data);
}

#endif /* PROVEN_NO_U16STR */
