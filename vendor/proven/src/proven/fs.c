#include "proven/fs.h"
#include "../../platform/proven_sys_fs.h"
#include "../../platform/proven_sys_io.h"
#include "proven/array.h"
#include "proven/algorithm.h"

/* Starting capacity when the source reports no usable size (streams, /proc). */
#define INTERNAL_SLURP_CHUNK ((proven_size_t)65536)

typedef struct {
    proven_err_t err;
    char *value;
} internal_result_cstr_t;

static bool view_has_nul(proven_u8str_view_t view) {
    if (view.size > 0 && !view.ptr) return true;
    for (proven_size_t i = 0; i < view.size; ++i) {
        if (view.ptr[i] == 0) return true;
    }
    return false;
}

static internal_result_cstr_t internal_view_to_cstr(proven_allocator_t scratch, proven_u8str_view_t view) {
    if (!proven_alloc_is_valid(scratch)) return (internal_result_cstr_t){.err = PROVEN_ERR_INVALID_ARG};
    if (view.size == 0) return (internal_result_cstr_t){.err = PROVEN_ERR_INVALID_ARG};
    if (view_has_nul(view)) return (internal_result_cstr_t){.err = PROVEN_ERR_INVALID_ARG};
    proven_size_t cap;
    if (PROVEN_CKD_ADD(&cap, view.size, 1)) return (internal_result_cstr_t){.err = PROVEN_ERR_OVERFLOW};
    proven_result_mem_mut_t m_res = scratch.alloc_fn(scratch.ctx, cap, 1);
    if (!proven_is_ok(m_res.err)) return (internal_result_cstr_t){.err = m_res.err};
    char *buf = (char *)m_res.value.ptr;
    for (proven_size_t i = 0; i < view.size; ++i) buf[i] = (char)view.ptr[i];
    buf[view.size] = '\0';
    return (internal_result_cstr_t){.err = PROVEN_OK, .value = buf};
}

static void internal_cstr_free(proven_allocator_t scratch, char *buf) {
    if (buf && proven_alloc_is_valid(scratch)) {
        scratch.free_fn(scratch.ctx, buf);
    }
}

proven_result_file_t proven_fs_open(proven_allocator_t scratch, proven_u8str_view_t path, proven_fs_mode_t mode) {
    proven_result_file_t res = {0};
    internal_result_cstr_t path_res = internal_view_to_cstr(scratch, path);
    if (!proven_is_ok(path_res.err)) {
        res.err = path_res.err;
        return res;
    }
    char *path_buf = path_res.value;

    int pal_flags = (int)mode;
    const int supported_flags = PROVEN_FS_READ | PROVEN_FS_WRITE | PROVEN_FS_APPEND | PROVEN_FS_CREATE | PROVEN_FS_TRUNC | PROVEN_FS_CREATE_NEW;
    if (pal_flags & ~supported_flags) {
        internal_cstr_free(scratch, path_buf);
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if ((pal_flags & PROVEN_FS_APPEND) && (pal_flags & PROVEN_FS_TRUNC)) {
        internal_cstr_free(scratch, path_buf);
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if ((pal_flags & PROVEN_FS_TRUNC) && !(pal_flags & (PROVEN_FS_WRITE | PROVEN_FS_APPEND))) {
        internal_cstr_free(scratch, path_buf);
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    // If no access bits are set, default to READ
    if (!(pal_flags & (PROVEN_FS_READ | PROVEN_FS_WRITE | PROVEN_FS_APPEND))) {
        pal_flags |= PROVEN_FS_READ;
    }

    proven_sys_file_handle_t sh = proven_sys_fs_open(path_buf, pal_flags);
    internal_cstr_free(scratch, path_buf);

#if defined(_WIN32) || defined(_WIN64)
    if (!sh.handle) {
        res.err = PROVEN_ERR_IO;
        return res;
    }
    res.value.internal.ptr = sh.handle;
#else
    if (sh.fd < 0) {
        res.err = PROVEN_ERR_IO;
        return res;
    }
    res.value.internal.fd = sh.fd;
#endif

    res.err = PROVEN_OK;
    return res;
}

proven_err_t proven_fs_close(proven_file_t file) {
#if defined(_WIN32) || defined(_WIN64)
    bool ok = proven_sys_fs_close((proven_sys_file_handle_t){ .handle = file.internal.ptr });
#else
    bool ok = proven_sys_fs_close((proven_sys_file_handle_t){ .fd = file.internal.fd });
#endif
    return ok ? PROVEN_OK : PROVEN_ERR_IO;
}

proven_result_size_t proven_fs_read(proven_file_t file, proven_mem_mut_t dest) {
    proven_result_size_t res = {0};
#if defined(_WIN32) || defined(_WIN64)
    if (!file.internal.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t sr = proven_sys_fs_read((proven_sys_file_handle_t){ .handle = file.internal.ptr }, dest.ptr, dest.size);
#else
    if (file.internal.fd < 0) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t sr = proven_sys_fs_read((proven_sys_file_handle_t){ .fd = file.internal.fd }, dest.ptr, dest.size);
#endif
    res.err = sr.err;
    res.value = sr.value;
    return res;
}

proven_result_size_t proven_fs_write(proven_file_t file, proven_mem_view_t src) {
    proven_result_size_t res = {0};
#if defined(_WIN32) || defined(_WIN64)
    if (!file.internal.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t sw = proven_sys_fs_write((proven_sys_file_handle_t){ .handle = file.internal.ptr }, src.ptr, src.size);
#else
    if (file.internal.fd < 0) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t sw = proven_sys_fs_write((proven_sys_file_handle_t){ .fd = file.internal.fd }, src.ptr, src.size);
#endif
    res.err = sw.err;
    res.value = sw.value;
    return res;
}

proven_err_t proven_fs_write_all(proven_file_t file, proven_mem_view_t src) {
#if defined(_WIN32) || defined(_WIN64)
    if (!file.internal.ptr) return PROVEN_ERR_INVALID_ARG;
#else
    if (file.internal.fd < 0) return PROVEN_ERR_INVALID_ARG;
#endif
    if (src.size > 0 && !src.ptr) return PROVEN_ERR_INVALID_ARG;
    
    proven_size_t total_written = 0;
    while (total_written < src.size) {
        proven_mem_view_t slice = { .ptr = src.ptr + total_written, .size = src.size - total_written };
        proven_result_size_t r = proven_fs_write(file, slice);
        if (!proven_is_ok(r.err)) return r.err;
        if (r.value == 0) return PROVEN_ERR_IO;
        total_written += r.value;
    }
    return PROVEN_OK;
}

proven_result_size_t proven_fs_size(proven_file_t file) {
    proven_result_size_t res = {0};
#if defined(_WIN32) || defined(_WIN64)
    if (!file.internal.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t sr = proven_sys_fs_size((proven_sys_file_handle_t){ .handle = file.internal.ptr });
#else
    if (file.internal.fd < 0) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t sr = proven_sys_fs_size((proven_sys_file_handle_t){ .fd = file.internal.fd });
#endif
    res.err = sr.err;
    res.value = sr.value;
    return res;
}


// -------------------------------------------------------------
// Position, size, and durability
// -------------------------------------------------------------

/* The file and console PALs use the same underlying handle; this is the one place
 * that says so, rather than each entry point re-deriving it. */
static proven_sys_io_handle_t internal_io_handle(proven_file_t file) {
#if defined(_WIN32) || defined(_WIN64)
    return (proven_sys_io_handle_t){ .handle = file.internal.ptr };
#else
    return (proven_sys_io_handle_t){ .fd = file.internal.fd };
#endif
}

static bool internal_file_is_valid(proven_file_t file) {
#if defined(_WIN32) || defined(_WIN64)
    return file.internal.ptr != 0;
#else
    return file.internal.fd >= 0;
#endif
}

proven_result_u64_t proven_fs_seek(proven_file_t file, proven_i64 offset, proven_fs_whence_t whence) {
    if (!internal_file_is_valid(file)) return (proven_result_u64_t){ PROVEN_ERR_INVALID_ARG, 0 };

    int w;
    switch (whence) {
        case PROVEN_FS_SEEK_SET: w = PROVEN_SYS_IO_SEEK_SET; break;
        case PROVEN_FS_SEEK_CUR: w = PROVEN_SYS_IO_SEEK_CUR; break;
        case PROVEN_FS_SEEK_END: w = PROVEN_SYS_IO_SEEK_END; break;
        default: return (proven_result_u64_t){ PROVEN_ERR_INVALID_ARG, 0 };
    }

    proven_sys_result_u64_t r = proven_sys_io_seek(internal_io_handle(file), (int64_t)offset, w);
    return (proven_result_u64_t){ r.err, (proven_u64)r.value };
}

proven_result_u64_t proven_fs_tell(proven_file_t file) {
    return proven_fs_seek(file, 0, PROVEN_FS_SEEK_CUR);
}

proven_err_t proven_fs_truncate(proven_file_t file, proven_u64 length) {
    if (!internal_file_is_valid(file)) return PROVEN_ERR_INVALID_ARG;
    return proven_sys_io_truncate(internal_io_handle(file), (uint64_t)length);
}

proven_result_size_t proven_fs_pread(proven_file_t file, proven_mem_mut_t dest, proven_u64 offset) {
    proven_result_size_t res = {0};
    if (!internal_file_is_valid(file)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (dest.size > 0 && !dest.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t r = proven_sys_io_pread(internal_io_handle(file), dest.ptr, dest.size, (uint64_t)offset);
    res.err = r.err;
    res.value = r.value;
    return res;
}

proven_result_size_t proven_fs_pwrite(proven_file_t file, proven_mem_view_t src, proven_u64 offset) {
    proven_result_size_t res = {0};
    if (!internal_file_is_valid(file)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (src.size > 0 && !src.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_sys_result_size_t r = proven_sys_io_pwrite(internal_io_handle(file), src.ptr, src.size, (uint64_t)offset);
    res.err = r.err;
    res.value = r.value;
    return res;
}

proven_err_t proven_fs_sync(proven_file_t file) {
    if (!internal_file_is_valid(file)) return PROVEN_ERR_INVALID_ARG;
    return proven_sys_io_sync(internal_io_handle(file));
}

proven_err_t proven_fs_sync_dir(proven_allocator_t scratch, proven_u8str_view_t path) {
#if defined(_WIN32) || defined(_WIN64)
    (void)scratch;
    (void)path;
    /* Windows has no directory handle that can be flushed like this. Say so rather
     * than returning OK and leaving the caller believing the rename is durable. */
    return PROVEN_ERR_UNSUPPORTED;
#else
    /* A directory has to be opened read-only to be fsync'd: opening it for writing
     * is EISDIR. */
    proven_result_file_t d = proven_fs_open(scratch, path, PROVEN_FS_READ);
    if (!proven_is_ok(d.err)) return d.err;

    proven_err_t err = proven_sys_io_sync(internal_io_handle(d.value));
    (void)proven_fs_close(d.value);
    return err;
#endif
}

proven_err_t proven_fs_rename(proven_allocator_t scratch, proven_u8str_view_t src, proven_u8str_view_t dest) {
    internal_result_cstr_t s_res = internal_view_to_cstr(scratch, src);
    if (!proven_is_ok(s_res.err)) return s_res.err;
    
    internal_result_cstr_t d_res = internal_view_to_cstr(scratch, dest);
    if (!proven_is_ok(d_res.err)) {
        internal_cstr_free(scratch, s_res.value);
        return d_res.err;
    }

    bool success = proven_sys_fs_rename(s_res.value, d_res.value);
    internal_cstr_free(scratch, s_res.value);
    internal_cstr_free(scratch, d_res.value);
    return success ? PROVEN_OK : PROVEN_ERR_IO;
}

proven_err_t proven_fs_remove(proven_allocator_t scratch, proven_u8str_view_t path) {
    internal_result_cstr_t p_res = internal_view_to_cstr(scratch, path);
    if (!proven_is_ok(p_res.err)) return p_res.err;
    
    bool success = proven_sys_fs_remove(p_res.value);
    internal_cstr_free(scratch, p_res.value);
    return success ? PROVEN_OK : PROVEN_ERR_IO;
}

proven_err_t proven_fs_mkdir(proven_allocator_t scratch, proven_u8str_view_t path) {
    internal_result_cstr_t p_res = internal_view_to_cstr(scratch, path);
    if (!proven_is_ok(p_res.err)) return p_res.err;
    
    bool success = proven_sys_fs_mkdir(p_res.value);
    internal_cstr_free(scratch, p_res.value);
    return success ? PROVEN_OK : PROVEN_ERR_IO;
}

proven_err_t proven_fs_rmdir(proven_allocator_t scratch, proven_u8str_view_t path) {
    internal_result_cstr_t p_res = internal_view_to_cstr(scratch, path);
    if (!proven_is_ok(p_res.err)) return p_res.err;
    
    bool success = proven_sys_fs_rmdir(p_res.value);
    internal_cstr_free(scratch, p_res.value);
    return success ? PROVEN_OK : PROVEN_ERR_IO;
}

proven_err_t proven_fs_copy(proven_allocator_t temp_alloc, proven_u8str_view_t src, proven_u8str_view_t dest) {
    if (!proven_alloc_is_valid(temp_alloc)) return PROVEN_ERR_INVALID_ARG;

    // Prevent copying a file to itself (identity check via stat)
    {
        proven_fs_stat_t s_stat, d_stat;
        proven_err_t s_err = proven_fs_stat(temp_alloc, src, &s_stat);
        proven_err_t d_err = proven_fs_stat(temp_alloc, dest, &d_stat);
        
        // If both exist and point to the same physical file (same dev and inode/fileindex)
        if (proven_is_ok(s_err) && proven_is_ok(d_err)) {
            if (s_stat.dev == d_stat.dev && s_stat.ino == d_stat.ino) {
                // If they are the same file, copying is a no-op that risks truncation if we proceed.
                // We return PROVEN_OK if they are identical as requested by standard behavior (cp fails, but no-op is often acceptable)
                // Actually most 'cp' tools return error if src and dest are same.
                return PROVEN_ERR_INVALID_ARG;
            }
        }
    }

    proven_result_file_t r_res = proven_fs_open(temp_alloc, src, PROVEN_FS_READ);
    if (!proven_is_ok(r_res.err)) return r_res.err;
    
    /*
     * An existing destination we cannot WRITE to has to be made writable first.
     *
     * The copy carries the source's mode onto the destination (see below), so copying a
     * 0400 file leaves a 0400 file - and the NEXT copy onto it could not even open it:
     * open(O_WRONLY) on a 0400 file fails, so a backup loop worked once and failed forever
     * after, with the destination silently keeping its old contents. We are about to
     * overwrite the file anyway; making it writable first is the honest thing, and the
     * final mode goes back on at the end.
     */
    {
        proven_fs_stat_t d_stat = {0};
        if (proven_is_ok(proven_fs_stat(temp_alloc, dest, &d_stat)) &&
            d_stat.type == PROVEN_FS_TYPE_FILE &&
            (d_stat.perms & 0200u) == 0u) {
            (void)proven_fs_chmod(temp_alloc, dest, (proven_fs_perms_t)(d_stat.perms | 0600u));
        }
    }

    proven_result_file_t w_res = proven_fs_open(temp_alloc, dest, PROVEN_FS_WRITE | PROVEN_FS_CREATE | PROVEN_FS_TRUNC);
    if (!proven_is_ok(w_res.err)) {
        (void)proven_fs_close(r_res.value);
        return w_res.err;
    }

    /*
     * Take the source's mode, and narrow the destination to owner-only for the duration of
     * the write.
     *
     * A copy used to create the destination with the process umask - so copying a 0600 file
     * produced a 0644 one, and every byte of a private file was world-readable from the
     * moment it was written. `cp` does not do that, and neither should this. Writing under
     * 0600 and setting the real mode at the end means the contents are never visible under
     * a wider mode than the original had, not even briefly, AND the file stays writable
     * while we are writing it.
     */
    proven_fs_stat_t src_stat = {0};
    bool have_src_perms = proven_is_ok(proven_fs_stat(temp_alloc, src, &src_stat)) &&
                          src_stat.type == PROVEN_FS_TYPE_FILE;
    if (have_src_perms) {
        proven_err_t merr = proven_fs_chmod(temp_alloc, dest, (proven_fs_perms_t)0600u);
        if (!proven_is_ok(merr) && merr != PROVEN_ERR_UNSUPPORTED) {
            (void)proven_fs_close(r_res.value);
            (void)proven_fs_close(w_res.value);
            return merr;
        }
    }

    proven_size_t buf_size = 4096 * 16;
    proven_result_mem_mut_t m_res = temp_alloc.alloc_fn(temp_alloc.ctx, buf_size, 8);
    if (!proven_is_ok(m_res.err)) {
        (void)proven_fs_close(r_res.value);
        (void)proven_fs_close(w_res.value);
        return m_res.err;
    }

    proven_err_t final_err = PROVEN_OK;
    while (true) {
        proven_result_size_t read_bytes = proven_fs_read(r_res.value, m_res.value);
        if (read_bytes.err == PROVEN_ERR_EOF) {
            break;
        }
        if (!proven_is_ok(read_bytes.err) || read_bytes.value == 0) {
            final_err = read_bytes.err;
            break;
        }
        proven_mem_view_t v = { .ptr = m_res.value.ptr, .size = read_bytes.value };
        proven_err_t werr = proven_fs_write_all(w_res.value, v);
        if (!proven_is_ok(werr)) {
            final_err = werr;
            break;
        }
    }

    temp_alloc.free_fn(temp_alloc.ctx, m_res.value.ptr);
    (void)proven_fs_close(r_res.value);   /* a read close cannot lose data */
    {
        /* The destination close CAN: on NFS or over quota this is where the failure
         * appears, and a copy that reports success with a truncated destination is
         * worse than one that fails. */
        proven_err_t cerr = proven_fs_close(w_res.value);
        if (proven_is_ok(final_err) && !proven_is_ok(cerr)) final_err = cerr;
    }

    /* The real mode goes on now that the bytes are all in and the fd is closed. */
    if (proven_is_ok(final_err) && have_src_perms) {
        proven_err_t merr = proven_fs_chmod(temp_alloc, dest, src_stat.perms);
        if (!proven_is_ok(merr) && merr != PROVEN_ERR_UNSUPPORTED) final_err = merr;
    }
    return final_err;
}

static int compare_fs_entries(const void *a, const void *b) {
    const proven_fs_entry_t *ea = a;
    const proven_fs_entry_t *eb = b;
    // Dirs first, then name
    if (ea->type != eb->type) {
        if (ea->type == PROVEN_FS_TYPE_DIR) return -1;
        if (eb->type == PROVEN_FS_TYPE_DIR) return 1;
    }
    proven_u8str_view_t va = proven_u8str_as_view(&ea->name);
    proven_u8str_view_t vb = proven_u8str_as_view(&eb->name);
    
    proven_size_t min_len = va.size < vb.size ? va.size : vb.size;
    for (proven_size_t i = 0; i < min_len; ++i) {
        if (va.ptr[i] < vb.ptr[i]) return -1;
        if (va.ptr[i] > vb.ptr[i]) return 1;
    }
    if (va.size < vb.size) return -1;
    if (va.size > vb.size) return 1;
    return 0;
}

proven_result_array_t proven_fs_list(proven_allocator_t alloc, proven_u8str_view_t path) {
    proven_result_array_t res = {0};
    internal_result_cstr_t p_res = internal_view_to_cstr(alloc, path);
    if (!proven_is_ok(p_res.err)) {
        res.err = p_res.err;
        return res;
    }

    proven_sys_dir_handle_t dh = proven_sys_fs_dir_open(p_res.value);
    internal_cstr_free(alloc, p_res.value);
    
    if (!dh.internal) {
        res.err = PROVEN_ERR_IO;
        return res;
    }

    proven_result_array_t a_res = PROVEN_ARRAY_INIT(alloc, proven_fs_entry_t, 16);
    if (!proven_is_ok(a_res.err)) {
        proven_sys_fs_dir_close(dh);
        return a_res;
    }

    proven_sys_dir_entry_t se;
    int step;
    while ((step = proven_sys_fs_dir_step(dh, &se)) == 1) {
        proven_fs_entry_t entry = {0};
        entry.type = se.is_dir ? PROVEN_FS_TYPE_DIR
                  : se.is_regular ? PROVEN_FS_TYPE_FILE
                  : PROVEN_FS_TYPE_OTHER;
        entry.size = se.size;
        
        proven_u8str_view_t name_view = { .ptr = (const proven_u8*)se.name, .size = 0 };
        while (se.name[name_view.size]) name_view.size++;
        
        proven_result_u8str_t s_res = proven_u8str_create_from_view(alloc, name_view);
        if (!proven_is_ok(s_res.err)) {
            proven_sys_fs_dir_close(dh);
            proven_fs_list_destroy(alloc, &a_res.value);
            res.err = s_res.err;
            return res;
        }
        entry.name = s_res.value;
        
        proven_err_t push_err = proven_array_push(&a_res.value, &entry);
        if (!proven_is_ok(push_err)) {
            proven_u8str_destroy(alloc, &entry.name);
            proven_sys_fs_dir_close(dh);
            proven_fs_list_destroy(alloc, &a_res.value);
            res.err = push_err;
            return res;
        }
    }
    proven_sys_fs_dir_close(dh);
    if (step < 0) {
        /* Half a listing reported as a whole one is how a backup silently skips files. */
        proven_fs_list_destroy(alloc, &a_res.value);
        res.err = PROVEN_ERR_IO;
        return res;
    }

    // Sort entries: directories first, then alphabetical
    proven_array_sort(&a_res.value, compare_fs_entries);

    res.err = PROVEN_OK;
    res.value = a_res.value;
    return res;
}

void proven_fs_list_destroy(proven_allocator_t alloc, proven_array_t *list) {
    if (!list || !list->data) return;
    proven_size_t count = list->len;
    proven_fs_entry_t *entries = (proven_fs_entry_t*)list->data;
    for (proven_size_t i = 0; i < count; ++i) {
        proven_u8str_destroy(alloc, &entries[i].name);
    }
    proven_array_destroy(list);
}

/*
 * Reads `f` to EOF, taking ownership of the caller's `cap`-byte allocation and
 * growing it as needed. On success the returned buffer holds every byte the
 * source produced; on failure the buffer is freed and nothing leaks.
 *
 * Reading to EOF rather than to a pre-measured size is what makes this correct
 * for two cases the size alone cannot express: sources whose size is unknowable
 * up front (pipes, FIFOs, /proc entries, character devices - proven_fs_size
 * reports 0 for anything that is not a regular file), and regular files that
 * grow while they are being read.
 *
 * The caller keeps ownership of `f`; this never closes it.
 *
 * `reserve_extra` asks for one spare byte beyond the payload, so callers that
 * need a NUL terminator do not force a second allocation.
 */
typedef struct {
    proven_err_t   err;
    proven_byte_t *ptr;
    proven_size_t  len;   /* bytes actually read */
    proven_size_t  cap;   /* live allocation size */
} internal_slurp_t;

static internal_slurp_t internal_read_to_eof(proven_allocator_t alloc,
                                             proven_file_t f,
                                             proven_byte_t *ptr,
                                             proven_size_t cap,
                                             proven_size_t align) {
    internal_slurp_t res = {0};
    proven_size_t len = 0;

    for (;;) {
        if (len == cap) {
            /* The buffer is exactly full. That is the common case for a regular
             * file whose size we were told, and it does NOT mean there is more
             * to read - but we cannot know that without asking. Ask with a
             * single byte on the stack rather than by doubling the buffer first:
             * growing here would double the allocation for every regular file in
             * existence, purely to discover that the file ended where its size
             * said it would. */
            proven_byte_t probe;
            proven_mem_mut_t one = { .ptr = &probe, .size = 1 };
            proven_result_size_t p = proven_fs_read(f, one);
            if (p.err == PROVEN_ERR_EOF || (proven_is_ok(p.err) && p.value == 0)) break;
            if (!proven_is_ok(p.err)) {
                alloc.free_fn(alloc.ctx, ptr);
                res.err = p.err;
                return res;
            }

            /* The source really does have more than its size promised. Now grow,
             * and keep the byte the probe already consumed. */
            proven_size_t new_cap;
            if (PROVEN_CKD_MUL(&new_cap, cap, (proven_size_t)2)) {
                alloc.free_fn(alloc.ctx, ptr);
                res.err = PROVEN_ERR_OVERFLOW;
                return res;
            }
            proven_result_mem_mut_t grow = alloc.realloc_fn(alloc.ctx, ptr, cap, new_cap, align);
            if (!proven_is_ok(grow.err)) {
                /* realloc is failure-atomic: `ptr` is still the live allocation. */
                alloc.free_fn(alloc.ctx, ptr);
                res.err = grow.err;
                return res;
            }
            ptr = grow.value.ptr;
            cap = new_cap;
            ptr[len++] = probe;
        }

        proven_mem_mut_t slice = { .ptr = ptr + len, .size = cap - len };
        proven_result_size_t r = proven_fs_read(f, slice);
        if (r.err == PROVEN_ERR_EOF) break;
        if (!proven_is_ok(r.err)) {
            alloc.free_fn(alloc.ctx, ptr);
            res.err = r.err;
            return res;
        }
        if (r.value == 0) break;
        len += r.value; /* r.value <= cap - len, so this cannot overflow */
    }

    res.err = PROVEN_OK;
    res.ptr = ptr;
    res.len = len;
    res.cap = cap;
    return res;
}

/*
 * Opens `path` and reads it to EOF into an owned buffer.
 *
 * `extra` reserves spare bytes past the payload (a NUL terminator for the
 * u8str variant), so the string form needs no second allocation.
 *
 * Reading to EOF rather than to a pre-measured size is what makes this correct
 * for two cases a size alone cannot express: sources whose size is unknowable up
 * front (pipes, FIFOs, /proc entries, character devices - proven_fs_size reports
 * 0 for anything that is not a regular file), and regular files that grow while
 * they are being read. proven_fs_size still seeds the initial capacity, so a
 * regular file of known size is read in a single allocation and a single pass.
 */
static internal_slurp_t internal_slurp_path(proven_allocator_t alloc,
                                            proven_u8str_view_t path,
                                            proven_size_t extra,
                                            proven_size_t align) {
    internal_slurp_t res = {0};

    proven_result_file_t f_res = proven_fs_open(alloc, path, PROVEN_FS_READ);
    if (!proven_is_ok(f_res.err)) {
        res.err = f_res.err;
        return res;
    }
    proven_file_t f = f_res.value;

    proven_result_size_t s_res = proven_fs_size(f);
    if (!proven_is_ok(s_res.err)) {
        (void)proven_fs_close(f);
        res.err = s_res.err;
        return res;
    }

    /* Base the fallback on the reported size, not on the capacity: with extra=1
     * the capacity is never 0, so testing the capacity would leave a size-0
     * source (a pipe, a /proc entry) starting from a one-byte buffer and
     * doubling its way up. */
    proven_size_t cap;
    if (s_res.value == 0) {
        cap = INTERNAL_SLURP_CHUNK;
    } else if (PROVEN_CKD_ADD(&cap, s_res.value, extra)) {
        (void)proven_fs_close(f);
        res.err = PROVEN_ERR_OVERFLOW;
        return res;
    }

    proven_result_mem_mut_t m_res = alloc.alloc_fn(alloc.ctx, cap, align);
    if (!proven_is_ok(m_res.err)) {
        (void)proven_fs_close(f);
        res.err = m_res.err;
        return res;
    }

    res = internal_read_to_eof(alloc, f, m_res.value.ptr, cap, align);
    (void)proven_fs_close(f);
    return res;
}

proven_result_mem_mut_t proven_fs_read_all(proven_allocator_t alloc, proven_u8str_view_t path) {
    proven_result_mem_mut_t res = {0};
    if (!proven_alloc_is_valid(alloc)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    internal_slurp_t s = internal_slurp_path(alloc, path, 0, 1);
    if (!proven_is_ok(s.err)) {
        res.err = s.err;
        return res;
    }

    if (s.len == 0) {
        alloc.free_fn(alloc.ctx, s.ptr);
        res.err = PROVEN_OK;
        res.value = (proven_mem_mut_t){ .ptr = (proven_byte_t*)0, .size = 0 };
        return res;
    }

    /* Give back the slack the size estimate over-reserved. A failed shrink is
     * not an error: the larger allocation is still valid and correctly sized. */
    if (s.len < s.cap && alloc.realloc_fn) {
        proven_result_mem_mut_t shrink = alloc.realloc_fn(alloc.ctx, s.ptr, s.cap, s.len, 1);
        if (proven_is_ok(shrink.err)) {
            s.ptr = shrink.value.ptr;
        }
    }

    res.err = PROVEN_OK;
    res.value.ptr = s.ptr;
    res.value.size = s.len;
    return res;
}

proven_result_u8str_t proven_fs_read_all_u8str(proven_allocator_t alloc, proven_u8str_view_t path) {
    proven_result_u8str_t res = {0};
    if (!proven_alloc_is_valid(alloc)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    /* +1 so the NUL that proven_u8str_as_cstr relies on always has room.
     *
     * PROVEN_DEFAULT_ALIGNMENT, not 1: the caller may go on to grow or destroy
     * this string through the normal u8str entry points, which realloc and free
     * it at that alignment. A block must be reallocated with the alignment it
     * was allocated with, so the string this returns has to be allocated exactly
     * as proven_u8str_create would have allocated it. */
    internal_slurp_t s = internal_slurp_path(alloc, path, 1, PROVEN_DEFAULT_ALIGNMENT);
    if (!proven_is_ok(s.err)) {
        res.err = s.err;
        return res;
    }

    if (s.len == s.cap) {
        /* The source filled the buffer exactly - it had one more byte than its
         * size promised. Make room for the terminator. */
        proven_size_t new_cap;
        if (PROVEN_CKD_ADD(&new_cap, s.cap, (proven_size_t)1)) {
            alloc.free_fn(alloc.ctx, s.ptr);
            res.err = PROVEN_ERR_OVERFLOW;
            return res;
        }
        proven_result_mem_mut_t grow = alloc.realloc_fn(alloc.ctx, s.ptr, s.cap, new_cap, PROVEN_DEFAULT_ALIGNMENT);
        if (!proven_is_ok(grow.err)) {
            alloc.free_fn(alloc.ctx, s.ptr);
            res.err = grow.err;
            return res;
        }
        s.ptr = grow.value.ptr;
        s.cap = new_cap;
    } else if (s.len + 1 < s.cap) {
        /* Give back the slack a chunked read over-reserved. A failed shrink is
         * not an error: the larger allocation is still valid. */
        proven_result_mem_mut_t shrink = alloc.realloc_fn(alloc.ctx, s.ptr, s.cap, s.len + 1, PROVEN_DEFAULT_ALIGNMENT);
        if (proven_is_ok(shrink.err)) {
            s.ptr = shrink.value.ptr;
            s.cap = s.len + 1;
        }
    }

    s.ptr[s.len] = 0;

    res.err = PROVEN_OK;
    res.value.borrowed = false;
    res.value.internal.ptr = s.ptr;
    res.value.internal.len = s.len;
    res.value.internal.cap = s.cap;
    return res;
}

proven_err_t proven_fs_write_file(proven_allocator_t scratch, proven_u8str_view_t path, proven_mem_view_t data) {
    if (!proven_alloc_is_valid(scratch)) return PROVEN_ERR_INVALID_ARG;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;

    proven_result_file_t f_res = proven_fs_open(scratch, path,
        (proven_fs_mode_t)(PROVEN_FS_WRITE | PROVEN_FS_CREATE | PROVEN_FS_TRUNC));
    if (!proven_is_ok(f_res.err)) return f_res.err;

    proven_err_t err = proven_fs_write_all(f_res.value, data);

    /* The close is part of the write. A filesystem that buffered the data and then could
     * not store it says so HERE - and this function used to throw that away and return
     * PROVEN_OK. */
    proven_err_t cerr = proven_fs_close(f_res.value);
    if (proven_is_ok(err)) err = cerr;
    return err;
}

/* Longest basename most filesystems accept. The temp sibling has to fit too. */
#define INTERNAL_NAME_MAX ((proven_size_t)255)
#define INTERNAL_TMP_SUFFIX_LEN ((proven_size_t)8)   /* ".pvtmpNN", no NUL */

/*
 * Writes `data` to a sibling temp file and renames it over `path`.
 *
 * The rename is what readers observe: they see either the whole old file or the
 * whole new one, never a half-written prefix. The temp file is a sibling so the
 * rename stays within one filesystem, which is what makes it atomic.
 *
 * If the target already exists, its permissions are copied onto the temp file
 * before the rename. Without that, the new file would carry the temp file's
 * fresh 0666 & ~umask - so atomically rewriting a 0600 key file would publish it
 * as 0644. A rewrite must not widen access to what it rewrites.
 *
 * This is atomic with respect to concurrent readers, not durable across a power
 * loss - proven exposes no fsync, so the rename may reach the disk before the
 * data does. A caller that needs crash durability needs more than this library
 * currently offers.
 */

/* The directory a path lives in - "." when the path has no separator. Used to sync
 * the directory after a rename, which is the only thing that makes the rename
 * itself survive a power cut. */
static proven_u8str_view_t internal_parent_dir(proven_u8str_view_t path) {
    proven_size_t cut = PROVEN_INDEX_NOT_FOUND;
    for (proven_size_t i = 0; i < path.size; ++i) {
        if (path.ptr[i] == (proven_byte_t)'/' || path.ptr[i] == (proven_byte_t)'\\') cut = i;
    }
    if (cut == PROVEN_INDEX_NOT_FOUND) return PROVEN_LIT(".");
    if (cut == 0) return PROVEN_LIT("/");           /* "/foo" -> the root */
    return (proven_u8str_view_t){ .ptr = path.ptr, .size = cut };
}

static proven_err_t internal_write_file_atomic(proven_allocator_t scratch, proven_u8str_view_t path, proven_mem_view_t data, bool durable) {
    if (!proven_alloc_is_valid(scratch)) return PROVEN_ERR_INVALID_ARG;
    if (data.size > 0 && !data.ptr) return PROVEN_ERR_INVALID_ARG;
    if (path.size == 0 || !path.ptr) return PROVEN_ERR_INVALID_ARG;
    if (view_has_nul(path)) return PROVEN_ERR_INVALID_ARG;

    /* The temp name is "<path>.pvtmpNN". A basename may legally run right up to
     * NAME_MAX, and write_file would accept it - so trim the copied basename by
     * however much the suffix needs rather than producing a name the filesystem
     * will reject. The trimmed stem is only ever a temp file, and CREATE_NEW
     * still guarantees we never open one somebody else owns. */
    proven_size_t stem = path.size;
    proven_size_t name_start = 0;
    for (proven_size_t i = 0; i < path.size; ++i) {
        if (path.ptr[i] == (proven_byte_t)'/' || path.ptr[i] == (proven_byte_t)'\\') name_start = i + 1;
    }
    proven_size_t name_len = path.size - name_start;
    if (name_len + INTERNAL_TMP_SUFFIX_LEN > INTERNAL_NAME_MAX) {
        stem = name_start + (INTERNAL_NAME_MAX - INTERNAL_TMP_SUFFIX_LEN);
    }

    proven_size_t tmp_cap;
    if (PROVEN_CKD_ADD(&tmp_cap, stem, INTERNAL_TMP_SUFFIX_LEN + 1)) return PROVEN_ERR_OVERFLOW;

    proven_result_mem_mut_t tmp_mem = scratch.alloc_fn(scratch.ctx, tmp_cap, 1);
    if (!proven_is_ok(tmp_mem.err)) return tmp_mem.err;
    proven_byte_t *tmp = tmp_mem.value.ptr;
    for (proven_size_t i = 0; i < stem; ++i) tmp[i] = path.ptr[i];

    proven_u8str_view_t tmp_view = { .ptr = tmp, .size = stem + INTERNAL_TMP_SUFFIX_LEN };

    /* Read the target's permissions before we create anything, so we can carry
     * them across. A missing target is fine: then the temp's own fresh mode is
     * the right answer, exactly as it would be for write_file. */
    proven_fs_stat_t target = {0};
    bool have_target_perms = proven_is_ok(proven_fs_stat(scratch, path, &target)) &&
                             target.type == PROVEN_FS_TYPE_FILE;

    proven_result_file_t f_res = {0};
    f_res.err = PROVEN_ERR_BUSY;

    /* A handful of attempts, not a hundred: this loop exists to step over a temp
     * name a concurrent writer already holds, and every attempt costs an open()
     * and a path allocation. proven_fs_open collapses errno, so a persistent
     * failure (no write permission on the directory, say) looks the same as a
     * collision - and there is no point paying for it a hundred times. */
    for (int attempt = 0; attempt < 8; ++attempt) {
        proven_byte_t *s = tmp + stem;
        s[0] = '.'; s[1] = 'p'; s[2] = 'v'; s[3] = 't'; s[4] = 'm'; s[5] = 'p';
        s[6] = (proven_byte_t)('0' + (attempt / 10));
        s[7] = (proven_byte_t)('0' + (attempt % 10));
        s[8] = 0;

        f_res = proven_fs_open(scratch, tmp_view,
            (proven_fs_mode_t)(PROVEN_FS_WRITE | PROVEN_FS_CREATE_NEW));
        if (proven_is_ok(f_res.err)) break;
    }
    if (!proven_is_ok(f_res.err)) {
        scratch.free_fn(scratch.ctx, tmp);
        return f_res.err;
    }

    /*
     * Put the mode on the temp BEFORE writing a single byte of the payload.
     *
     * It used to be chmod'd after the write, at the very end - which meant the entire new
     * contents of a 0600 file sat in a world-readable 0644 temp for the whole duration of
     * the write. A watcher thread stat'ing the temp during a 64 MiB rewrite saw exactly
     * that. The end state was right and the window was wide open, and a window is all a
     * secret needs. If there is no target, the temp's fresh mode is the right answer, the
     * same as for write_file.
     */
    proven_err_t err = PROVEN_OK;
    if (have_target_perms) {
        err = proven_fs_chmod(scratch, tmp_view, target.perms);
    }

    if (proven_is_ok(err)) {
        err = proven_fs_write_all(f_res.value, data);
    }

    if (proven_is_ok(err) && durable) {
        /* The data has to be on the disk BEFORE the rename makes it visible under
         * the target name. Rename first and sync after, and a power cut in between
         * leaves the name pointing at a file whose contents never arrived - which
         * is exactly the corruption an atomic write exists to prevent. */
        err = proven_fs_sync(f_res.value);
    }

    /* And the close can fail too, which means the temp does not hold what we think it
     * holds. Renaming it over the target would then publish content the filesystem
     * refused to write - the exact corruption an atomic write exists to prevent. */
    {
        proven_err_t cerr = proven_fs_close(f_res.value);
        if (proven_is_ok(err)) err = cerr;
    }

    if (proven_is_ok(err)) {
        err = proven_fs_rename(scratch, tmp_view, path);
    }

    if (proven_is_ok(err) && durable) {
        /* And the rename itself is only durable once the DIRECTORY has reached the
         * disk. Syncing the file alone leaves a window where the bytes are safe and
         * the name that points at them is not.
         *
         * A platform that cannot do this says so; it does not get to fail the write
         * over it, because the write did happen and is atomic - it is only the
         * crash-durability of the rename that is unavailable. */
        proven_u8str_view_t dir = internal_parent_dir(path);
        proven_err_t derr = proven_fs_sync_dir(scratch, dir);
        if (!proven_is_ok(derr) && derr != PROVEN_ERR_UNSUPPORTED) {
            err = derr;
        }
    }

    if (!proven_is_ok(err)) {
        /* Leave no debris behind; the remove itself cannot rescue the error. */
        (void)proven_fs_remove(scratch, tmp_view);
    }

    scratch.free_fn(scratch.ctx, tmp);
    return err;
}

proven_err_t proven_fs_write_file_atomic(proven_allocator_t scratch, proven_u8str_view_t path, proven_mem_view_t data) {
    return internal_write_file_atomic(scratch, path, data, false);
}

proven_err_t proven_fs_write_file_durable(proven_allocator_t scratch, proven_u8str_view_t path, proven_mem_view_t data) {
    return internal_write_file_atomic(scratch, path, data, true);
}


// -------------------------------------------------------------
// Streaming directory iteration
// -------------------------------------------------------------

proven_result_dir_t proven_fs_dir_open(proven_allocator_t scratch, proven_u8str_view_t path) {
    proven_result_dir_t res = {0};
    internal_result_cstr_t p_res = internal_view_to_cstr(scratch, path);
    if (!proven_is_ok(p_res.err)) {
        res.err = p_res.err;
        return res;
    }

    proven_sys_dir_handle_t dh = proven_sys_fs_dir_open(p_res.value);
    internal_cstr_free(scratch, p_res.value);

    if (!dh.internal) {
        res.err = PROVEN_ERR_NOT_FOUND;
        return res;
    }
    res.err = PROVEN_OK;
    res.value.internal = dh.internal;
    return res;
}

proven_err_t proven_fs_dir_next(proven_fs_dir_t *dir, proven_fs_dir_entry_t *out_entry) {
    if (!dir || !dir->internal || !out_entry) return PROVEN_ERR_INVALID_ARG;

    proven_sys_dir_entry_t se = {0};
    proven_sys_dir_handle_t dh = { .internal = dir->internal };
    int step = proven_sys_fs_dir_step(dh, &se);
    if (step == 0) return PROVEN_ERR_EOF;
    if (step < 0) return PROVEN_ERR_IO;   /* a failed read is not an empty directory */

    /* Borrowed: the name points into the iterator's own storage, which is what lets a
     * huge directory be walked without an allocation per entry. */
    out_entry->name = proven_u8str_view_from_cstr(se.name);
    /* A symlink, FIFO, socket or device is neither. It used to be reported as a regular
     * file, which told the caller it could open it and read bytes out of it - and a
     * dangling symlink cannot even be opened. */
    out_entry->type = se.is_dir ? PROVEN_FS_TYPE_DIR
                    : se.is_regular ? PROVEN_FS_TYPE_FILE
                    : PROVEN_FS_TYPE_OTHER;
    out_entry->is_symlink = se.is_symlink;
    out_entry->size = se.size;
    return PROVEN_OK;
}

void proven_fs_dir_close(proven_fs_dir_t *dir) {
    if (!dir || !dir->internal) return;
    proven_sys_fs_dir_close((proven_sys_dir_handle_t){ .internal = dir->internal });
    dir->internal = (void *)0;
}

proven_err_t proven_fs_chmod(proven_allocator_t scratch, proven_u8str_view_t path, proven_fs_perms_t perms) {
    const proven_fs_perms_t supported_perms =
        PROVEN_FS_PERM_OWNER_R | PROVEN_FS_PERM_OWNER_W | PROVEN_FS_PERM_OWNER_X |
        PROVEN_FS_PERM_GROUP_R | PROVEN_FS_PERM_GROUP_W | PROVEN_FS_PERM_GROUP_X |
        PROVEN_FS_PERM_OTHER_R | PROVEN_FS_PERM_OTHER_W | PROVEN_FS_PERM_OTHER_X;
    if (((proven_fs_perms_t)perms & ~supported_perms) != 0) {
        return PROVEN_ERR_INVALID_ARG;
    }
    internal_result_cstr_t p_res = internal_view_to_cstr(scratch, path);
    if (!proven_is_ok(p_res.err)) return p_res.err;
    
    bool success = proven_sys_fs_chmod(p_res.value, (unsigned int)perms);
    internal_cstr_free(scratch, p_res.value);
    return success ? PROVEN_OK : PROVEN_ERR_IO;
}

proven_err_t proven_fs_lock(proven_file_t file, proven_fs_lock_type_t type, bool wait) {
    int t = 0;
    if (type == PROVEN_FS_LOCK_SHARED) {
        t = 0;
    } else if (type == PROVEN_FS_LOCK_EXCLUSIVE) {
        t = 1;
    } else if (type == PROVEN_FS_LOCK_UNLOCK) {
        t = 2;
    } else {
        return PROVEN_ERR_INVALID_ARG;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    return proven_sys_fs_lock((proven_sys_file_handle_t){ .handle = file.internal.ptr }, t, wait) ? PROVEN_OK : PROVEN_ERR_IO;
#else
    return proven_sys_fs_lock((proven_sys_file_handle_t){ .fd = file.internal.fd }, t, wait) ? PROVEN_OK : PROVEN_ERR_IO;
#endif
}

proven_err_t proven_fs_stat(proven_allocator_t scratch, proven_u8str_view_t path, proven_fs_stat_t *out_stat) {
    if (!out_stat) return PROVEN_ERR_INVALID_ARG;
    
    internal_result_cstr_t p_res = internal_view_to_cstr(scratch, path);
    if (!proven_is_ok(p_res.err)) return p_res.err;
    
    proven_sys_fs_stat_t se;
    bool stat_ok = proven_sys_fs_stat(p_res.value, &se);
    internal_cstr_free(scratch, p_res.value);
    
    if (!stat_ok) return PROVEN_ERR_IO;
    
    out_stat->size = se.size;
    out_stat->type = se.is_dir ? PROVEN_FS_TYPE_DIR
                   : se.is_regular ? PROVEN_FS_TYPE_FILE
                   : PROVEN_FS_TYPE_OTHER;
    /* Permission bits only. se.mode is the raw st_mode, which also carries the
     * file-type bits (S_IFREG and friends). Handing those back in a field typed
     * proven_fs_perms_t broke the most obvious use of it: feeding a stat's perms
     * straight to proven_fs_chmod, which rejects any bit outside the nine it
     * supports and so returned PROVEN_ERR_INVALID_ARG for every real file.
     * The proven_fs_perms_t bits are laid out exactly as the POSIX low nine. */
    out_stat->perms = (proven_fs_perms_t)(se.mode & 0777u);
    out_stat->modified_at = se.mtime;
    out_stat->created_at = 0; // standard stat doesn't provide birth time on all posix
    out_stat->dev = se.dev;
    out_stat->ino = se.ino;
    out_stat->uid = se.uid;
    out_stat->gid = se.gid;

    return PROVEN_OK;
}

proven_err_t proven_fs_symlink(proven_allocator_t scratch, proven_u8str_view_t target, proven_u8str_view_t linkpath) {
    internal_result_cstr_t t_res = internal_view_to_cstr(scratch, target);
    if (!proven_is_ok(t_res.err)) return t_res.err;

    internal_result_cstr_t l_res = internal_view_to_cstr(scratch, linkpath);
    if (!proven_is_ok(l_res.err)) {
        internal_cstr_free(scratch, t_res.value);
        return l_res.err;
    }
    
    bool success = proven_sys_fs_symlink(t_res.value, l_res.value);
    internal_cstr_free(scratch, t_res.value);
    internal_cstr_free(scratch, l_res.value);
    return success ? PROVEN_OK : PROVEN_ERR_IO;
}

proven_err_t proven_fs_link(proven_allocator_t scratch, proven_u8str_view_t oldpath, proven_u8str_view_t newpath) {
    internal_result_cstr_t o_res = internal_view_to_cstr(scratch, oldpath);
    if (!proven_is_ok(o_res.err)) return o_res.err;

    internal_result_cstr_t n_res = internal_view_to_cstr(scratch, newpath);
    if (!proven_is_ok(n_res.err)) {
        internal_cstr_free(scratch, o_res.value);
        return n_res.err;
    }

    bool success = proven_sys_fs_link(o_res.value, n_res.value);
    internal_cstr_free(scratch, o_res.value);
    internal_cstr_free(scratch, n_res.value);
    return success ? PROVEN_OK : PROVEN_ERR_IO;
}

bool proven_fs_is_absolute(proven_u8str_view_t path) {
    if (path.size == 0 || !path.ptr) return false;
#define PROVEN_FS_IS_SEP(c) ((c) == '/' || (c) == '\\')
    // POSIX root
    if (path.ptr[0] == '/') return true;
    // Windows UNC, device, and extended prefixes.
    if (path.size >= 2 && PROVEN_FS_IS_SEP(path.ptr[0]) && PROVEN_FS_IS_SEP(path.ptr[1])) return true;
    // Windows drive root (e.g., C:/)
    if (path.size >= 3 && 
        ((path.ptr[0] >= 'a' && path.ptr[0] <= 'z') || (path.ptr[0] >= 'A' && path.ptr[0] <= 'Z')) &&
        path.ptr[1] == ':' &&
        PROVEN_FS_IS_SEP(path.ptr[2])) {
        return true;
    }
#undef PROVEN_FS_IS_SEP
    return false;
}

// -------------------------------------------------------------
// Recursive walk
//
// Written to the contract in include/proven/fs.h, whose test (tests/test_unit_fs_walk)
// landed red one commit earlier. Nothing that test asserts was changed to make this pass.
// -------------------------------------------------------------

typedef struct {
    proven_fs_dir_t     dir;
    proven_size_t       path_len;   /* length of `path` when this level was entered */
    unsigned long long  dev;
    unsigned long long  ino;
} proven_fs_walk_frame_t;

typedef struct {
    proven_allocator_t     alloc;
    proven_size_t          max_depth;

    /* ONE path buffer, reused and only ever grown: the walk costs no allocation per entry,
     * which is what makes its memory a function of DEPTH rather than of breadth. */
    proven_byte_t         *path;
    proven_size_t          path_len;
    proven_size_t          path_cap;
    proven_fs_walk_frame_t stack[PROVEN_FS_WALK_DEPTH_LIMIT];
    proven_size_t          depth;   /* number of open frames */

    /*
     * The path buffer currently holds the entry we handed to the caller last time - their
     * `path` view points into it, and it must stay valid until they ask for the next one.
     * So the work of moving on happens at the START of the next call:
     *
     *   pending_descend: that entry was a directory we are going to enter.
     *   pending_trim:    that entry was not, and the path goes back to its parent.
     *
     * `pending_path_len` is the parent's length in both cases.
     */
    bool                   pending_descend;
    bool                   pending_trim;
    proven_size_t          pending_path_len;
} proven_fs_walk_state_t;

static proven_u8str_view_t walk_path(const proven_fs_walk_state_t *s) {
    return (proven_u8str_view_t){ .ptr = s->path, .size = s->path_len };
}

static proven_err_t walk_reserve(proven_fs_walk_state_t *s, proven_size_t need) {
    if (need <= s->path_cap) return PROVEN_OK;

    proven_size_t cap = s->path_cap ? s->path_cap : 128u;
    while (cap < need) {
        if (PROVEN_CKD_MUL(&cap, cap, 2u)) return PROVEN_ERR_OVERFLOW;
    }

    proven_result_mem_mut_t m = s->alloc.realloc_fn(s->alloc.ctx, s->path, s->path_cap, cap, 1u);
    if (!proven_is_ok(m.err)) return m.err;   /* realloc is failure-atomic: the old block lives */

    s->path = (proven_byte_t *)m.value.ptr;
    s->path_cap = cap;
    return PROVEN_OK;
}

/* Append "/name". */
static proven_err_t walk_push_name(proven_fs_walk_state_t *s, proven_u8str_view_t name) {
    proven_size_t need;
    if (PROVEN_CKD_ADD(&need, s->path_len, name.size) || PROVEN_CKD_ADD(&need, need, 2u)) {
        return PROVEN_ERR_OVERFLOW;   /* '/' and the NUL the path needs to be openable */
    }
    proven_err_t e = walk_reserve(s, need);
    if (!proven_is_ok(e)) return e;

    s->path[s->path_len++] = (proven_byte_t)'/';
    for (proven_size_t i = 0; i < name.size; ++i) s->path[s->path_len + i] = name.ptr[i];
    s->path_len += name.size;
    s->path[s->path_len] = 0;
    return PROVEN_OK;
}

static void walk_truncate(proven_fs_walk_state_t *s, proven_size_t len) {
    if (len <= s->path_len) {
        s->path_len = len;
        if (s->path && s->path_cap > len) s->path[len] = 0;
    }
}

/* Is (dev, ino) already an ancestor on the current path? That is a cycle, and it is the
 * only thing standing between this walk and an infinite loop through `loop -> ..`. */
static bool walk_is_ancestor(const proven_fs_walk_state_t *s, unsigned long long dev, unsigned long long ino) {
    for (proven_size_t i = 0; i < s->depth; ++i) {
        if (s->stack[i].dev == dev && s->stack[i].ino == ino) return true;
    }
    return false;
}

proven_result_walk_t proven_fs_walk_open(proven_allocator_t alloc, proven_u8str_view_t root,
                                         proven_size_t max_depth) {
    proven_result_walk_t res = {0};

    if (!proven_alloc_is_valid(alloc) || root.size == 0 || !root.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    proven_result_mem_mut_t mem = alloc.alloc_fn(alloc.ctx, sizeof(proven_fs_walk_state_t),
                                                 alignof(proven_fs_walk_state_t));
    if (!proven_is_ok(mem.err)) {
        res.err = mem.err;
        return res;
    }

    proven_fs_walk_state_t *s = (proven_fs_walk_state_t *)mem.value.ptr;
    for (proven_size_t i = 0; i < sizeof *s; ++i) ((proven_byte_t *)s)[i] = 0;
    s->alloc = alloc;
    s->max_depth = max_depth;

    proven_err_t perr = walk_reserve(s, root.size + 2u);
    if (!proven_is_ok(perr)) {
        alloc.free_fn(alloc.ctx, s);
        res.err = perr;
        return res;
    }
    for (proven_size_t i = 0; i < root.size; ++i) s->path[i] = root.ptr[i];
    s->path_len = root.size;
    s->path[s->path_len] = 0;

    proven_result_dir_t d = proven_fs_dir_open(alloc, root);
    if (!proven_is_ok(d.err)) {
        alloc.free_fn(alloc.ctx, s->path);
        alloc.free_fn(alloc.ctx, s);
        res.err = d.err;
        return res;
    }

    proven_fs_stat_t st = {0};
    (void)proven_fs_stat(alloc, root, &st);   /* dev/ino of the root: the first ancestor */

    s->stack[0].dir = d.value;
    s->stack[0].path_len = s->path_len;
    s->stack[0].dev = st.dev;
    s->stack[0].ino = st.ino;
    s->depth = 1;

    res.err = PROVEN_OK;
    res.value.internal = s;
    return res;
}

proven_err_t proven_fs_walk_next(proven_fs_walk_t *walk, proven_fs_walk_entry_t *out_entry) {
    if (!walk || !walk->internal || !out_entry) return PROVEN_ERR_INVALID_ARG;
    proven_fs_walk_state_t *s = (proven_fs_walk_state_t *)walk->internal;

    for (;;) {
        if (s->pending_trim) {
            /* The caller is done with the entry we handed them last time. */
            s->pending_trim = false;
            walk_truncate(s, s->pending_path_len);
        }

        /* A directory reported on the previous call is entered now - after the caller has
         * seen it, which is what pre-order means. */
        if (s->pending_descend) {
            s->pending_descend = false;

            proven_u8str_view_t dpath = walk_path(s);

            if (s->depth >= PROVEN_FS_WALK_DEPTH_LIMIT) {
                /*
                 * Deeper than the walk's own stack can hold. This IS an error, and saying so
                 * is the whole point: the first version just stopped descending, so a tree
                 * 300 levels deep came back with 256 entries and a clean PROVEN_ERR_EOF -
                 * the walk had hidden a subtree and reported success, which is precisely the
                 * failure this API's contract condemns. It does not get to commit it.
                 */
                out_entry->path = dpath;
                out_entry->name = dpath;
                for (proven_size_t i = dpath.size; i > 0; --i) {
                    if (dpath.ptr[i - 1] == (proven_u8)'/') {
                        out_entry->name.ptr = dpath.ptr + i;
                        out_entry->name.size = dpath.size - i;
                        break;
                    }
                }
                out_entry->type = PROVEN_FS_TYPE_DIR;
                out_entry->size = 0;
                out_entry->depth = s->depth - 1;
                out_entry->is_symlink = false;

                s->pending_trim = true;
                return PROVEN_ERR_OUT_OF_BOUNDS;
            }

            /*
             * Open the child RELATIVE TO the parent's open handle, refusing to follow a
             * symlink - not by re-opening the reconstructed path string.
             *
             * That string could be swapped out from under us: an entry that was a real
             * directory when it was listed can be a symlink-to-outside by the time we go to
             * enter it, and opening it by path would follow the link and walk the whole
             * filesystem. The audit's TOCTOU repro did exactly that. openat(parent, name,
             * O_NOFOLLOW) cannot be fooled that way - if `name` is a symlink now, the open
             * fails - and it is what makes "cannot escape" true rather than merely likely.
             *
             * We also take dev/ino from the handle we actually opened (fstat), not from a
             * path, so the cycle guard is checking the real directory too.
             */
            proven_fs_dir_t child = {0};
            unsigned long long cdev = 0, cino = 0;
            bool have_ids = false;
            proven_err_t open_err;

            if (proven_sys_fs_dir_supports_open_at()) {
                proven_size_t nlen = dpath.size - s->pending_path_len - 1u;
                const proven_u8 *nptr = dpath.ptr + s->pending_path_len + 1u;
                /* dpath is NUL-terminated at path_len; the name component is too, because it
                 * is the tail of it. Pass it as a C string. */
                proven_sys_dir_handle_t parent = { .internal = s->stack[s->depth - 1].dir.internal };
                proven_sys_dir_handle_t ch = proven_sys_fs_dir_open_at(parent, (const char *)nptr);
                (void)nlen; (void)nptr;
                if (ch.internal) {
                    child.internal = ch.internal;
                    have_ids = proven_sys_fs_dir_ids(ch, &cdev, &cino);
                    open_err = PROVEN_OK;
                } else {
                    open_err = PROVEN_ERR_IO;
                }
            } else {
                /* No fd-relative open on this platform: fall back to the by-path open, and
                 * stat the path for the cycle guard's ids - the one place that stat is worth
                 * paying for, rather than on every descent as it used to be. */
                proven_result_dir_t d = proven_fs_dir_open(s->alloc, dpath);
                child = d.value;
                open_err = d.err;
                if (proven_is_ok(open_err)) {
                    proven_fs_stat_t st = {0};
                    have_ids = proven_is_ok(proven_fs_stat(s->alloc, dpath, &st));
                    cdev = st.dev;
                    cino = st.ino;
                }
            }

            /* Re-check the cycle against what we actually opened. On a bind mount or a
             * hardlinked directory the openat succeeds and only the (dev, ino) tells us it
             * is an ancestor. */
            if (proven_is_ok(open_err) && have_ids && walk_is_ancestor(s, cdev, cino)) {
                proven_fs_dir_close(&child);
                walk_truncate(s, s->pending_path_len);
                continue;
            }

            if (!proven_is_ok(open_err)) {
                /* A directory we cannot read, or one that turned into a symlink under us.
                 * REPORT it - a subtree that silently vanishes is how a backup misses files
                 * and says it succeeded - and carry on with the next sibling. */
                out_entry->path = dpath;
                out_entry->name = dpath;   /* narrowed below */
                for (proven_size_t i = dpath.size; i > 0; --i) {
                    if (dpath.ptr[i - 1] == (proven_u8)'/') {
                        out_entry->name.ptr = dpath.ptr + i;
                        out_entry->name.size = dpath.size - i;
                        break;
                    }
                }
                out_entry->type = PROVEN_FS_TYPE_DIR;
                out_entry->size = 0;
                out_entry->depth = s->depth - 1;
                out_entry->is_symlink = false;

                /* Trim on the NEXT call, not now: the caller is holding a view into this
                 * buffer, and truncating writes a NUL into the middle of what they are
                 * looking at. */
                s->pending_trim = true;
                return open_err;
            }

            s->stack[s->depth].dir = child;
            s->stack[s->depth].path_len = s->pending_path_len;
            s->stack[s->depth].dev = cdev;
            s->stack[s->depth].ino = cino;
            s->depth++;
            continue;
        }

        if (s->depth == 0) return PROVEN_ERR_EOF;

        proven_fs_walk_frame_t *top = &s->stack[s->depth - 1];
        proven_fs_dir_entry_t de = {0};
        proven_err_t e = proven_fs_dir_next(&top->dir, &de);

        if (e == PROVEN_ERR_EOF) {
            proven_fs_dir_close(&top->dir);
            walk_truncate(s, top->path_len);
            s->depth--;
            continue;
        }
        if (!proven_is_ok(e)) {
            /*
             * The directory's own read failed mid-iteration. Report it, name it, move on -
             * and describe it the SAME way the other two error branches do, which this one
             * used not to. `name` is the last component, not the whole path; and the depth
             * is the directory's OWN depth (the one it was reported at as an entry), not its
             * children's. We are inside the frame here, so that is s->depth - 2, where the
             * open-failure branch - which is one level shallower, before the frame is pushed
             * - correctly uses s->depth - 1.
             */
            proven_u8str_view_t dpath = walk_path(s);
            out_entry->path = dpath;
            out_entry->name = dpath;
            for (proven_size_t i = dpath.size; i > 0; --i) {
                if (dpath.ptr[i - 1] == (proven_u8)'/') {
                    out_entry->name.ptr = dpath.ptr + i;
                    out_entry->name.size = dpath.size - i;
                    break;
                }
            }
            out_entry->type = PROVEN_FS_TYPE_DIR;
            out_entry->size = 0;
            out_entry->depth = (s->depth >= 2) ? s->depth - 2 : 0;
            out_entry->is_symlink = false;

            proven_fs_dir_close(&top->dir);
            s->pending_trim = true;
            s->pending_path_len = top->path_len;
            s->depth--;
            return e;
        }

        proven_size_t base_len = s->path_len;
        proven_err_t perr = walk_push_name(s, de.name);
        if (!proven_is_ok(perr)) {
            walk_truncate(s, base_len);
            return perr;
        }

        proven_u8str_view_t full = walk_path(s);
        proven_size_t depth = s->depth - 1;

        out_entry->path = full;
        out_entry->name.ptr = full.ptr + base_len + 1u;
        out_entry->name.size = full.size - base_len - 1u;
        out_entry->type = de.type;
        out_entry->size = de.size;
        out_entry->depth = depth;
        out_entry->is_symlink = de.is_symlink;

        s->pending_path_len = base_len;
        if (de.type == PROVEN_FS_TYPE_DIR && !de.is_symlink &&
            (s->max_depth == PROVEN_FS_WALK_UNLIMITED || depth < s->max_depth)) {
            /* Report it now, enter it next time: that is what pre-order means. */
            s->pending_descend = true;
            s->pending_trim = false;
        } else {
            /* A directory AT the limit is still reported - it is an entry - it is simply
             * not entered. Same for a file. Either way the path goes back to the parent's
             * at the start of the next call, not now: the caller is holding a view into it. */
            s->pending_descend = false;
            s->pending_trim = true;
        }
        return PROVEN_OK;
    }
}

void proven_fs_walk_close(proven_fs_walk_t *walk) {
    if (!walk || !walk->internal) return;
    proven_fs_walk_state_t *s = (proven_fs_walk_state_t *)walk->internal;

    for (proven_size_t i = 0; i < s->depth; ++i) {
        proven_fs_dir_close(&s->stack[i].dir);
    }
    proven_allocator_t a = s->alloc;
    if (s->path) a.free_fn(a.ctx, s->path);
    a.free_fn(a.ctx, s);
    walk->internal = NULL;
}
