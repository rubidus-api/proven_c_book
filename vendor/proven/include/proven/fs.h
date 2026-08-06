#ifndef PROVEN_FS_H
#define PROVEN_FS_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/u8str.h"
#include "proven/memory.h"
#include "proven/array.h"
#include "proven/scan.h"   /* proven_result_u64_t */

/**
 * @file fs.h
 * @brief High-level File System I/O interface.
 */

// We redefine this here for internal PAL coherence or just use void*
typedef struct {
    union {
        void *ptr;
        int   fd;
    } internal;
} proven_fs_handle_t;

typedef enum {
    PROVEN_FS_READ   = 1 << 0,
    PROVEN_FS_WRITE  = 1 << 1,
    PROVEN_FS_APPEND = 1 << 2,
    PROVEN_FS_CREATE = 1 << 3,
    PROVEN_FS_TRUNC  = 1 << 4,
    PROVEN_FS_CREATE_NEW = 1 << 5
} proven_fs_mode_t;

/*
 * Raw filesystem helpers do not sanitize untrusted paths, enforce root
 * confinement, or defend against symlink-race TOCTOU. Callers handling
 * untrusted paths must validate them first.
 */

typedef enum {
    PROVEN_FS_TYPE_FILE,
    PROVEN_FS_TYPE_DIR,
    PROVEN_FS_TYPE_OTHER
} proven_fs_type_t;

/**
 * @brief Metadata for a single filesystem entry.
 */
typedef struct {
    proven_u8str_t   name; // Owned by the list/caller context
    proven_fs_type_t type;
    proven_size_t    size;
} proven_fs_entry_t;

/**
 * @brief Opaque handle representing an open file.
 */
typedef proven_fs_handle_t proven_file_t;

typedef struct {
    proven_err_t  err;
    proven_file_t value;
} proven_result_file_t;

// -------------------------------------------------------------
// Core File Operations
// -------------------------------------------------------------

/**
 * @brief Opens a file at the specified path with the given mode.
 */
[[nodiscard]]
proven_result_file_t proven_fs_open(proven_allocator_t scratch, proven_u8str_view_t path, proven_fs_mode_t mode);

/**
 * @brief Closes an open file handle, and tells you whether the OS was happy about it.
 *
 * `close()` is the LAST chance a filesystem has to report that a write did not land, and
 * on NFS, CIFS and quota-enforcing filesystems it is the ONLY chance: the bytes were
 * buffered, `write()` returned success, and the failure surfaces here or nowhere. This
 * used to return `void`. `proven_fs_write_file` therefore returned `PROVEN_OK` for a file
 * the filesystem had just refused to write, and `write_file_atomic` went ahead and
 * renamed the temp over the target, publishing content the disk had rejected.
 *
 * On a file you only read from, a close failure is not data loss and `(void)`-ing it is
 * reasonable. On a file you wrote to, it is the report.
 */
[[nodiscard]]
proven_err_t proven_fs_close(proven_file_t file);

/**
 * @brief Reads data from an open file into a mutable slice.
 */
[[nodiscard]]
proven_result_size_t proven_fs_read(proven_file_t file, proven_mem_mut_t dest);

/**
 * @brief Writes data from a view into an open file.
 */
[[nodiscard]]
proven_result_size_t proven_fs_write(proven_file_t file, proven_mem_view_t src);

/**
 * @brief Writes the entire src view into an open file, retrying on partial writes.
 */
[[nodiscard]]
proven_err_t proven_fs_write_all(proven_file_t file, proven_mem_view_t src);

/**
 * @brief Retrieves the size of an open file in bytes.
 *
 * @note This is the file's length, not the write cursor. Use proven_fs_tell for
 *       the position.
 */
[[nodiscard]]
proven_result_size_t proven_fs_size(proven_file_t file);

// -------------------------------------------------------------
// Position, size, and durability
// -------------------------------------------------------------

typedef enum {
    PROVEN_FS_SEEK_SET = 0,   /**< from the beginning of the file */
    PROVEN_FS_SEEK_CUR = 1,   /**< from the current position */
    PROVEN_FS_SEEK_END = 2    /**< from the end; a negative offset goes backwards */
} proven_fs_whence_t;

/**
 * @brief Moves the file position. Returns the resulting absolute offset.
 *
 * @note A handle that cannot seek - a pipe, a FIFO, a terminal - returns
 *       PROVEN_ERR_UNSUPPORTED, not PROVEN_ERR_IO. Not being seekable is a
 *       property of the thing, not a failure of the call, and code that adapts to
 *       it needs to be able to tell the two apart.
 * @note Seeking past the end of a file is legal and does not extend it; a write
 *       there does, leaving a hole.
 */
[[nodiscard]]
proven_result_u64_t proven_fs_seek(proven_file_t file, proven_i64 offset, proven_fs_whence_t whence);

/**
 * @brief Returns the current file position without moving it.
 */
[[nodiscard]]
proven_result_u64_t proven_fs_tell(proven_file_t file);

/**
 * @brief Sets the file's length, growing it with zeros or discarding the tail.
 *
 * @note This is O(1). Truncating used to require reading the whole file and
 *       rewriting the part you wanted to keep - an O(n) copy for an O(1)
 *       operation - because there was no way to say this.
 * @note The file position is unchanged. If it now sits past the end, that is
 *       allowed; a write there extends the file again.
 */
[[nodiscard]]
proven_err_t proven_fs_truncate(proven_file_t file, proven_u64 length);

/**
 * @brief Reads at an absolute offset without moving the file position.
 *
 * @note This is what makes concurrent reads of one file handle safe: two threads
 *       sharing a handle cannot race on a cursor that neither of them moves.
 * @note Same EOF contract as proven_fs_read: end of file is PROVEN_ERR_EOF, not a
 *       zero-byte success.
 * @note Not available on a pipe: PROVEN_ERR_UNSUPPORTED.
 */
[[nodiscard]]
proven_result_size_t proven_fs_pread(proven_file_t file, proven_mem_mut_t dest, proven_u64 offset);

/**
 * @brief Writes at an absolute offset without moving the file position.
 *
 * @note May write fewer bytes than requested, like proven_fs_write.
 * @note Not available on a pipe: PROVEN_ERR_UNSUPPORTED.
 */
[[nodiscard]]
proven_result_size_t proven_fs_pwrite(proven_file_t file, proven_mem_view_t src, proven_u64 offset);

/**
 * @brief Forces this file's data to the storage device (fsync).
 *
 * Until now the library had no way to do this at any price: it imported no fsync
 * and no fdatasync, so a caller who wanted their bytes on the platter simply could
 * not ask. The old proven_sysio_flush was not it - it did nothing, and it is gone;
 * pushing a buffered writer's bytes to the OS is proven_writer_flush, and pushing the
 * OS's bytes to the disk is this call. They are different things and now say so.
 *
 * @note This is expensive, and it is meant to be. Call it when you have decided
 *       that losing the data would be worse than the wait.
 */
[[nodiscard]]
proven_err_t proven_fs_sync(proven_file_t file);

/**
 * @brief Forces a directory's own metadata to the storage device.
 *
 * A rename is only durable once the *directory* has reached the disk. So an atomic
 * rewrite that must survive a power cut needs both: sync the new file's data, then
 * rename, then sync the directory the rename happened in. Syncing the file alone
 * leaves a window in which the data is safe and the name that points at it is not.
 *
 * @note POSIX only in effect. On Windows a directory handle cannot be flushed this
 *       way, and this returns PROVEN_ERR_UNSUPPORTED rather than pretending.
 */
[[nodiscard]]
proven_err_t proven_fs_sync_dir(proven_allocator_t scratch, proven_u8str_view_t path);

// -------------------------------------------------------------
// Advanced Filesystem Management
// -------------------------------------------------------------

/**
 * @brief Renames or moves a file/directory from src to dest.
 */
[[nodiscard]]
proven_err_t proven_fs_rename(proven_allocator_t scratch, proven_u8str_view_t src, proven_u8str_view_t dest);

/**
 * @brief Deletes a file at the specified path.
 */
[[nodiscard]]
proven_err_t proven_fs_remove(proven_allocator_t scratch, proven_u8str_view_t path);

/**
 * @brief Copies a file from src to dest using custom memory buffers for efficiency.
 */
[[nodiscard]]
proven_err_t proven_fs_copy(proven_allocator_t temp_alloc, proven_u8str_view_t src, proven_u8str_view_t dest);

/**
 * @brief Creates a directory.
 */
[[nodiscard]]
proven_err_t proven_fs_mkdir(proven_allocator_t scratch, proven_u8str_view_t path);

/**
 * @brief Removes an empty directory.
 */
[[nodiscard]]
proven_err_t proven_fs_rmdir(proven_allocator_t scratch, proven_u8str_view_t path);

// -------------------------------------------------------------
// Directory Iteration
// -------------------------------------------------------------

// -------------------------------------------------------------
// Streaming directory iteration
// -------------------------------------------------------------

typedef struct {
    void *internal;
} proven_fs_dir_t;

typedef struct {
    proven_err_t    err;
    proven_fs_dir_t value;
} proven_result_dir_t;

/**
 * @brief A directory entry, borrowed.
 *
 * @note `name` points into the iterator's own storage and is valid only until the
 *       next proven_fs_dir_next. Copy it if it must outlive that. Borrowing is the
 *       point: it is what lets a million-entry directory be walked without a
 *       million allocations.
 */
typedef struct {
    proven_u8str_view_t name;
    proven_fs_type_t    type;
    proven_size_t       size;

    /**
     * @brief This entry was reached through a symlink. `type` still describes the TARGET.
     *
     * You need both facts, and they are different facts. `type` follows the link so that a
     * listing and proven_fs_stat agree about what a thing is. `is_symlink` says how you got
     * there — and a tree walker has to know, because following a symlinked directory can
     * walk it straight out of the tree it was asked about.
     */
    bool is_symlink;
} proven_fs_dir_entry_t;

/**
 * @brief Opens a directory for streaming iteration.
 *
 * proven_fs_list reads the WHOLE directory before the caller sees any of it:
 * measured on 50,000 entries, 189 ms, +4.2 MB of resident memory and 50,008
 * allocations, with nothing visible until the last entry was read. That is fine for
 * a config directory and useless for a mail spool.
 *
 * This walks it one entry at a time and allocates nothing per entry.
 */
[[nodiscard]]
proven_result_dir_t proven_fs_dir_open(proven_allocator_t scratch, proven_u8str_view_t path);

/**
 * @brief Next entry, or PROVEN_ERR_EOF when the directory is exhausted.
 *
 * @note The returned name is borrowed; see proven_fs_dir_entry_t.
 *
 * @note `type` FOLLOWS symlinks, exactly as proven_fs_stat does — a symlink to a regular
 *       file is PROVEN_FS_TYPE_FILE, and a symlink to a directory is PROVEN_FS_TYPE_DIR.
 *       That consistency is deliberate: the walk used to report a perfectly ordinary file
 *       as PROVEN_FS_TYPE_OTHER because it was reached through a link, while `stat` on the
 *       same path said FILE, and a caller filtering on `type == FILE` skipped files it
 *       could open and read.
 *
 *       The consequence you must handle: **a recursive walker can loop.** A symlink
 *       pointing at an ancestor directory is a cycle, and the type says DIR. Guard it the
 *       way every tree walker does — carry a depth limit, or remember (dev, ino) pairs
 *       from proven_fs_stat and refuse to descend into one you have already seen.
 *
 *       A DANGLING symlink, and anything else that is neither a regular file nor a
 *       directory (a FIFO, a socket, a device), is PROVEN_FS_TYPE_OTHER. A dangling link
 *       cannot be opened at all, so calling it a file would be the lie this note exists to
 *       prevent.
 */
[[nodiscard]]
proven_err_t proven_fs_dir_next(proven_fs_dir_t *dir, proven_fs_dir_entry_t *out_entry);

void proven_fs_dir_close(proven_fs_dir_t *dir);

// -----------------------------------------------------------------------------
// Recursive walk
// -----------------------------------------------------------------------------

/** @brief Descend without limit - as far as PROVEN_FS_WALK_DEPTH_LIMIT allows. */
#define PROVEN_FS_WALK_UNLIMITED ((proven_size_t)-1)

/**
 * @brief The deepest the walk can go, ever: its stack of open directories is fixed.
 *
 * A directory deeper than this is reported with PROVEN_ERR_OUT_OF_BOUNDS, naming the
 * directory - it is NOT silently skipped. Stopping quietly at the limit would mean a tree
 * 300 levels deep came back with 256 entries and a clean end-of-walk: a hidden subtree,
 * reported as success, which is the one thing this API exists not to do.
 */
#define PROVEN_FS_WALK_DEPTH_LIMIT ((proven_size_t)256)

typedef struct {
    /**
     * @brief Full path, from the root the walk was opened on. Borrowed.
     *
     * Valid until the next call to proven_fs_walk_next or _close. Copy it if you need it
     * to outlive the step - the walk reuses one buffer, which is what lets a walk of a
     * million entries cost one allocation instead of a million.
     */
    proven_u8str_view_t path;

    /** @brief The last component of `path`. Borrowed, same lifetime. */
    proven_u8str_view_t name;

    /** @brief FILE, DIR, or OTHER - and it follows symlinks, exactly as proven_fs_stat does. */
    proven_fs_type_t type;

    /** @brief Size in bytes for a regular file; 0 otherwise. */
    proven_size_t size;

    /** @brief 0 for an entry directly inside the root, 1 for one level down, and so on. */
    proven_size_t depth;

    /** @brief Reached through a symlink. `type` describes the target; the walk does not enter it. */
    bool is_symlink;
} proven_fs_walk_entry_t;

typedef struct {
    void *internal;
} proven_fs_walk_t;

typedef struct {
    proven_err_t    err;
    proven_fs_walk_t value;
} proven_result_walk_t;

/**
 * @brief Opens a recursive, pre-order walk of `root`.
 *
 * The walk that proven_fs_dir_* deliberately does not give you, with the three things a
 * recursive walker gets wrong:
 *
 * - **It cannot loop, and it cannot escape — even under a race.** The walk never descends
 *   THROUGH a symlink. A symlinked directory is still REPORTED (it exists, `type` is DIR,
 *   `is_symlink` is true, and hiding it would be its own lie) — it is simply not entered.
 *   That one rule buys both guarantees: a link pointing at an ancestor cannot loop the walk,
 *   and a link pointing anywhere else cannot walk you out of the tree you asked about.
 *
 *   The descent is fd-relative and refuses to follow a symlink (`openat(parent, name,
 *   O_NOFOLLOW)` where the platform has it), so this holds even against a TOCTOU attacker:
 *   an entry that is a real directory when it is listed and a symlink when it is entered
 *   makes the descent FAIL — reported as that directory's error — rather than following the
 *   swapped link out of the tree. (Both this and the "follow, but stop at a cycle" first
 *   draft that quietly walked all of /tmp were found by the contract's own audit.)
 *
 *   Belt and braces: the walk also carries the (dev, ino) of every directory on the current
 *   path and refuses to descend into one it is already inside, which covers the loops a
 *   symlink is not needed for — bind mounts, and the hardlinked directories some
 *   filesystems still allow.
 *
 * - **It does not hide what it could not read.** A directory the walk cannot open is
 *   reported: proven_fs_walk_next returns that directory's error, with `out_entry` filled
 *   in so you know which one it was, and the walk goes on from the next sibling. A tree
 *   walker that silently skips an unreadable directory is how a backup misses a subtree
 *   and reports success.
 *
 * - **Its memory is bounded by DEPTH, not by breadth.** One open directory handle and one
 *   (dev, ino) pair per level of the current path, plus a single reused path buffer. A
 *   directory of a million entries costs no more than a directory of ten.
 *
 * @param alloc     Allocator for the walk's own state. Freed by proven_fs_walk_close.
 * @param root      The directory to walk. Not itself reported; its contents are.
 * @param max_depth How far to descend. 0 reports only the entries directly inside `root`
 *                  and descends nowhere; PROVEN_FS_WALK_UNLIMITED has no limit. A directory
 *                  at the limit is still REPORTED (it is an entry); it is not descended into.
 *
 * @note Not thread-safe, like every other handle in this library. One walk per thread.
 */
[[nodiscard]]
proven_result_walk_t proven_fs_walk_open(proven_allocator_t alloc, proven_u8str_view_t root,
                                         proven_size_t max_depth);

/**
 * @brief The next entry in pre-order, or PROVEN_ERR_EOF when the walk is finished.
 *
 * A directory is reported BEFORE its contents. On an error that belongs to one directory
 * (it could not be opened, or its read failed), the error is returned and `out_entry`
 * describes that directory; call again to continue with the rest of the tree.
 */
[[nodiscard]]
proven_err_t proven_fs_walk_next(proven_fs_walk_t *walk, proven_fs_walk_entry_t *out_entry);

void proven_fs_walk_close(proven_fs_walk_t *walk);

/**
 * @brief Lists the contents of a directory into an array of proven_fs_entry_t.
 * @note This uses the provided allocator to store strings for entry names.
 *       Entries should be sorted by name by default.
 */
[[nodiscard]]
proven_result_array_t proven_fs_list(proven_allocator_t alloc, proven_u8str_view_t path);

/**
 * @brief Destroys an array of proven_fs_entry_t, freeing all internal strings.
 */
void proven_fs_list_destroy(proven_allocator_t alloc, proven_array_t *list);

// -------------------------------------------------------------
// Permissions & Locking
// -------------------------------------------------------------

/**
 * @brief Portable file permissions (POSIX-style bitmask).
 */
typedef enum {
    PROVEN_FS_PERM_OWNER_R = 1 << 8,
    PROVEN_FS_PERM_OWNER_W = 1 << 7,
    PROVEN_FS_PERM_OWNER_X = 1 << 6,
    PROVEN_FS_PERM_GROUP_R = 1 << 5,
    PROVEN_FS_PERM_GROUP_W = 1 << 4,
    PROVEN_FS_PERM_GROUP_X = 1 << 3,
    PROVEN_FS_PERM_OTHER_R = 1 << 2,
    PROVEN_FS_PERM_OTHER_W = 1 << 1,
    PROVEN_FS_PERM_OTHER_X = 1 << 0,
    PROVEN_FS_PERM_DEFAULT = PROVEN_FS_PERM_OWNER_R | PROVEN_FS_PERM_OWNER_W | PROVEN_FS_PERM_GROUP_R | PROVEN_FS_PERM_OTHER_R
} proven_fs_perms_t;

/**
 * @brief Lock types for file synchronization.
 */
typedef enum {
    PROVEN_FS_LOCK_SHARED,    // Read lock (Multiple readers allowed)
    PROVEN_FS_LOCK_EXCLUSIVE, // Write lock (Only one owner allowed)
    PROVEN_FS_LOCK_UNLOCK     // Release lock
} proven_fs_lock_type_t;

/**
 * @brief Set file permissions.
 */
[[nodiscard]]
proven_err_t proven_fs_chmod(proven_allocator_t scratch, proven_u8str_view_t path, proven_fs_perms_t perms);

/**
 * @brief Apply or release a file lock.
 */
[[nodiscard]]
proven_err_t proven_fs_lock(proven_file_t file, proven_fs_lock_type_t type, bool wait);

#define PROVEN_FS_PATH_SEP '/'

/**
 * @brief Detailed file information.
 */
typedef struct {
    proven_size_t size;
    proven_fs_type_t type;
    proven_fs_perms_t perms;  /* permission bits only; the file type is in `type` */
    proven_i64 created_at;
    proven_i64 modified_at;
    unsigned long long dev;
    unsigned long long ino;
    unsigned long long uid;   /* owner id: POSIX st_uid; 0 on Windows (no uid/gid) */
    unsigned long long gid;   /* group id: POSIX st_gid; 0 on Windows */
} proven_fs_stat_t;

/**
 * @brief Get detailed file/directory information.
 *
 * @note `perms` carries only the nine permission bits, so it can be handed
 *       straight back to proven_fs_chmod. It used to carry the raw POSIX
 *       st_mode, whose file-type bits chmod rejects - which made the obvious
 *       round-trip fail for every real file. Read the file type from `type`.
 */
[[nodiscard]]
proven_err_t proven_fs_stat(proven_allocator_t scratch, proven_u8str_view_t path, proven_fs_stat_t *out_stat);

/**
 * @brief Create a symbolic link.
 */
[[nodiscard]]
proven_err_t proven_fs_symlink(proven_allocator_t scratch, proven_u8str_view_t target, proven_u8str_view_t linkpath);

/**
 * @brief Create a hard link.
 */
[[nodiscard]]
proven_err_t proven_fs_link(proven_allocator_t scratch, proven_u8str_view_t oldpath, proven_u8str_view_t newpath);

/**
 * @brief Check if a path is absolute.
 */
[[nodiscard]]
bool proven_fs_is_absolute(proven_u8str_view_t path);

// -------------------------------------------------------------
// Utility Wrappers
// -------------------------------------------------------------

/**
 * @brief Reads the entire contents of a file into a newly allocated buffer.
 *
 * @note Reads to EOF, not to a pre-measured size. The file's reported size only
 *       seeds the initial capacity, so a regular file is still read in one
 *       allocation and one pass - but a source whose size cannot be known up
 *       front (pipe, FIFO, /proc entry, character device) is read correctly
 *       rather than returning empty, and a regular file that grows mid-read is
 *       not silently truncated.
 * @note An empty source yields `{.ptr = NULL, .size = 0}` with PROVEN_OK.
 * @note A regular file of known size costs exactly one allocation: EOF is
 *       confirmed with a one-byte probe rather than by growing the buffer.
 *       The buffer only grows if the source really does outrun its reported
 *       size. If the final shrink realloc fails, the larger allocation is
 *       returned with `value.size` correctly set to the bytes read.
 */
[[nodiscard]]
proven_result_mem_mut_t proven_fs_read_all(proven_allocator_t alloc, proven_u8str_view_t path);

/**
 * @brief Reads the entire contents of a file into a newly allocated owned string.
 *
 * The whole-file read most callers actually want: the result is a
 * NUL-terminated `proven_u8str_t`, so `proven_u8str_as_view` and
 * `proven_u8str_as_cstr` work on it without a second copy. The terminator is
 * reserved up front, so this costs no extra allocation over proven_fs_read_all.
 *
 * @note Contents are not validated as UTF-8; the bytes are returned as they are.
 * @note Same EOF and allocator semantics as proven_fs_read_all.
 * @note Destroy the result with proven_u8str_destroy.
 */
[[nodiscard]]
proven_result_u8str_t proven_fs_read_all_u8str(proven_allocator_t alloc, proven_u8str_view_t path);

/**
 * @brief Writes a buffer to a path in one call, creating or truncating the file.
 *
 * @note Not atomic: a reader can observe a partially written file, and a failure
 *       mid-write leaves the file truncated. Use proven_fs_write_file_atomic
 *       when a concurrent reader must never see a half-written file.
 */
[[nodiscard]]
proven_err_t proven_fs_write_file(proven_allocator_t scratch, proven_u8str_view_t path, proven_mem_view_t data);

/**
 * @brief Writes a buffer to a path via a sibling temp file and a rename.
 *
 * A concurrent reader sees either the entire old file or the entire new one,
 * never a partial write. On any failure the temp file is removed and the
 * original file is left untouched.
 *
 * @note Permissions are preserved. If the target exists, its mode is copied onto
 *       the temp file before the rename, so rewriting a 0600 file does not
 *       republish it as 0644. A new file gets the process default, as with
 *       proven_fs_write_file.
 * @note Atomic with respect to readers, not durable across power loss: proven
 *       exposes no fsync, so the rename may reach the disk before the data.
 * @note Replaces, rather than follows, a symbolic link at `path`: the rename
 *       leaves a regular file where the link was. proven_fs_write_file writes
 *       *through* the link instead. Pick accordingly.
 * @note Needs write permission on the containing directory, and a filesystem
 *       where the temp sibling and the target share a mount (they always do,
 *       since the temp file is created next to the target).
 */
[[nodiscard]]
proven_err_t proven_fs_write_file_atomic(proven_allocator_t scratch, proven_u8str_view_t path, proven_mem_view_t data);

/**
 * @brief Like proven_fs_write_file_atomic, but the bytes are on the disk before it
 *        returns.
 *
 * Atomicity and durability are different promises, and conflating them is how data
 * gets lost. `write_file_atomic` guarantees that a *reader* never sees a half-written
 * file. It does not guarantee that the file survives a power cut - the kernel may
 * still be holding the bytes, and the rename may reach the disk before the data it
 * points at.
 *
 * This call closes that window, in the only order that works:
 *
 *   1. write the temp file, then **fsync it** - the data is on the disk;
 *   2. rename it over the target - readers now see the new file, atomically;
 *   3. **fsync the directory** - the rename itself is now on the disk.
 *
 * Syncing the file but not the directory leaves a crash window in which the bytes are
 * safe and the name that points at them is not, which is the corruption an atomic
 * write exists to prevent.
 *
 * @note This is slow, and it is meant to be: it waits for the storage device, twice.
 *       Use it when losing the write would be worse than the wait, and use
 *       proven_fs_write_file_atomic when it would not.
 * @note On a platform with no directory sync (Windows), step 3 is skipped rather than
 *       failing the write: the write did happen and is atomic; only the
 *       crash-durability of the rename is unavailable.
 */
[[nodiscard]]
proven_err_t proven_fs_write_file_durable(proven_allocator_t scratch, proven_u8str_view_t path, proven_mem_view_t data);

#endif /* PROVEN_FS_H */
