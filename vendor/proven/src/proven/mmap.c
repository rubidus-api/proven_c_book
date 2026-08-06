#include "proven/mmap.h"
#include "../../platform/proven_sys_fs.h"

proven_result_mmap_t proven_mmap_create(proven_fs_handle_t file, proven_size_t offset, proven_size_t size, proven_mmap_prot_t prot, proven_mmap_flags_t flags) {
    if (flags != PROVEN_MMAP_PRIVATE && flags != PROVEN_MMAP_SHARED) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_INVALID_ARG, .value = {0} };
    }

    int valid_prot = PROVEN_MMAP_READ | PROVEN_MMAP_WRITE | PROVEN_MMAP_EXEC;
    if ((prot & ~valid_prot) != 0 || prot == 0) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_INVALID_ARG, .value = {0} };
    }

#if defined(_WIN32) || defined(_WIN64)
    if (prot & PROVEN_MMAP_EXEC) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_INVALID_ARG, .value = {0} };
    }
    proven_sys_file_handle_t sh = { .handle = file.internal.ptr };
#else
    proven_sys_file_handle_t sh = { .fd = file.internal.fd };
#endif
    proven_sys_result_size_t sr = proven_sys_fs_size(sh);
    if (!proven_is_ok(sr.err)) {
        return (proven_result_mmap_t){ .err = sr.err, .value = {0} };
    }

    proven_size_t offset_granularity = proven_sys_fs_mmap_offset_granularity();
    if (offset_granularity == 0 || (offset % offset_granularity) != 0) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_INVALID_ARG, .value = {0} };
    }

    if (offset >= (proven_size_t)sr.value) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_OUT_OF_BOUNDS, .value = {0} };
    }

    if (size == 0) {
        if (sr.value == 0) {
            return (proven_result_mmap_t){ .err = PROVEN_ERR_INVALID_ARG, .value = {0} };
        }
        size = ((proven_size_t)sr.value) - offset;
    }

    proven_size_t end;
    if (PROVEN_CKD_ADD(&end, offset, size)) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_OVERFLOW, .value = {0} };
    }

    if (end > (proven_size_t)sr.value) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_OUT_OF_BOUNDS, .value = {0} };
    }

    proven_sys_mmap_res_t res = proven_sys_fs_create(sh, (size_t)offset, (size_t)size, (int)prot, (int)flags);
    
    if (res.ptr == NULL) {
        return (proven_result_mmap_t){ .err = PROVEN_ERR_IO, .value = {0} };
    }

    proven_mmap_t m = {
        .ptr = res.ptr,
        .size = size,
        .file = file,
        .internal_handle = res.internal_handle,
        .flags = flags
    };

    return (proven_result_mmap_t){ .err = PROVEN_OK, .value = m };
}

proven_err_t proven_mmap_destroy(proven_mmap_t *mmap) {
    if (!mmap || !mmap->ptr) return PROVEN_ERR_INVALID_ARG;

    if (!proven_sys_fs_destroy(mmap->ptr, (size_t)mmap->size, mmap->internal_handle)) {
        return PROVEN_ERR_IO;
    }

    mmap->ptr = NULL;
    mmap->size = 0;
    return PROVEN_OK;
}

proven_err_t proven_mmap_sync(proven_mmap_t *mmap) {
    if (!mmap || !mmap->ptr) return PROVEN_ERR_INVALID_ARG;

    /*
     * A PRIVATE mapping is copy-on-write: your writes live in your page tables and nowhere
     * else, and msync() on it persists exactly nothing. It used to return PROVEN_OK -
     * "Synchronizes changes back to disk", said the header - so a caller could write
     * through a private mapping, sync it, be told it worked, and find the file unchanged.
     * Reporting success for a persist that cannot happen is the worst answer available.
     */
    if (mmap->flags == PROVEN_MMAP_PRIVATE) {
        return PROVEN_ERR_UNSUPPORTED;
    }

    if (!proven_sys_fs_sync(mmap->ptr, (size_t)mmap->size)) {
        return PROVEN_ERR_IO;
    }

    return PROVEN_OK;
}
