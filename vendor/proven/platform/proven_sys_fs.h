#ifndef PROVEN_PLATFORM_SYS_FS_H
#define PROVEN_PLATFORM_SYS_FS_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven_sys_io.h"
#include <stdbool.h>

/**
 * @file proven_sys_fs.h
 * @brief Platform Abstraction Layer for File System syscalls.
 */

typedef struct {
#if defined(_WIN32) || defined(_WIN64)
    void *handle;
#else
    int fd;
#endif
} proven_sys_file_handle_t;

typedef enum {
    PROVEN_SYS_FS_READ   = 1 << 0,
    PROVEN_SYS_FS_WRITE  = 1 << 1,
    PROVEN_SYS_FS_APPEND = 1 << 2,
    PROVEN_SYS_FS_CREATE = 1 << 3,
    PROVEN_SYS_FS_TRUNC  = 1 << 4,
    PROVEN_SYS_FS_CREATE_NEW = 1 << 5
} proven_sys_fs_mode_t;

[[nodiscard]]
proven_sys_file_handle_t proven_sys_fs_open(const char *path, int flags);

/**
 * @brief Closes the handle. Returns false if the OS reported an error.
 *
 * close() is the last chance the filesystem has to tell you a write did not land -
 * NFS, CIFS and quota-enforcing filesystems report exactly that, at exactly this
 * moment, and nowhere else. Discarding it is discarding the report.
 */
[[nodiscard]]
bool proven_sys_fs_close(proven_sys_file_handle_t handle);

[[nodiscard]]
proven_sys_result_size_t proven_sys_fs_read(proven_sys_file_handle_t handle, void *buf, size_t size);

[[nodiscard]]
proven_sys_result_size_t proven_sys_fs_write(proven_sys_file_handle_t handle, const void *buf, size_t size);

[[nodiscard]]
proven_sys_result_size_t proven_sys_fs_size(proven_sys_file_handle_t handle);

[[nodiscard]]
bool proven_sys_fs_rename(const char *src, const char *dest);

[[nodiscard]]
bool proven_sys_fs_remove(const char *path);

[[nodiscard]]
bool proven_sys_fs_mkdir(const char *path);

[[nodiscard]]
bool proven_sys_fs_rmdir(const char *path);

// Directory Iteration PAL
typedef struct {
    void *internal;
} proven_sys_dir_handle_t;

typedef struct {
    const char *name;
    bool is_dir;
    /* A symlink, a FIFO, a socket or a device is none of the two. Reporting it as a
     * regular file - which is what "not a directory" used to mean here - tells a caller
     * it can open it and read bytes out of it, and a dangling symlink cannot even be
     * opened. */
    bool is_regular;
    /* Reached through a symlink. `is_dir`/`is_regular` describe the TARGET (they follow, so
     * a listing agrees with stat) - but a walker still has to know it was a link, because
     * following one can leave the tree it was asked to walk. */
    bool is_symlink;
    size_t size;
} proven_sys_dir_entry_t;

[[nodiscard]]
proven_sys_dir_handle_t proven_sys_fs_dir_open(const char *path);

/**
 * @brief Open a subdirectory of an already-open directory, by name, WITHOUT following a
 *        symlink - the TOCTOU-safe descent a tree walker needs.
 *
 * `name` is a single path component (no slashes). The open is relative to `parent`'s own
 * file descriptor and refuses to traverse a symlink, so an entry that was a real directory
 * when it was listed and a symlink when it is entered cannot walk you out of the tree:
 * the open simply fails. Returns a NULL handle on any failure, including that one.
 *
 * Only meaningful where proven_sys_fs_dir_supports_open_at() is true (POSIX openat). On a
 * platform without it, the handle is always NULL and the caller must fall back to a
 * by-path open.
 */
[[nodiscard]]
proven_sys_dir_handle_t proven_sys_fs_dir_open_at(proven_sys_dir_handle_t parent, const char *name);

/** @brief True where proven_sys_fs_dir_open_at does a real fd-relative, no-follow open. */
[[nodiscard]]
bool proven_sys_fs_dir_supports_open_at(void);

/**
 * @brief The (dev, ino) of an open directory, from the handle itself - not from a path
 *        that could be swapped under us. Used for the walk's cycle guard.
 */
[[nodiscard]]
bool proven_sys_fs_dir_ids(proven_sys_dir_handle_t handle, unsigned long long *dev, unsigned long long *ino);

void proven_sys_fs_dir_close(proven_sys_dir_handle_t handle);

/**
 * @brief One step of a directory walk, with end-of-directory told apart from failure.
 *
 * @return 1 an entry was produced, 0 the directory ended, -1 the OS failed.
 *
 * readdir() returns NULL for both "no more entries" and "the read failed", and the
 * only thing that tells them apart is errno. Collapsing the two makes a truncated
 * listing - a directory on a failing disk, an NFS mount that went away - look exactly
 * like a complete one, which is the failure mode a filesystem library exists to prevent.
 */
[[nodiscard]]
int proven_sys_fs_dir_step(proven_sys_dir_handle_t handle, proven_sys_dir_entry_t *out_entry);

/** @brief Convenience wrapper: an entry was produced. Cannot report failure; prefer proven_sys_fs_dir_step. */
[[nodiscard]]
bool proven_sys_fs_dir_next(proven_sys_dir_handle_t handle, proven_sys_dir_entry_t *out_entry);

[[nodiscard]]
bool proven_sys_fs_chmod(const char *path, unsigned int perms);

[[nodiscard]]
bool proven_sys_fs_lock(proven_sys_file_handle_t handle, int type, bool wait);

typedef struct {
    size_t size;
    bool is_dir;
    bool is_regular;
    unsigned int mode;
    long long mtime;
    unsigned long long dev;
    unsigned long long ino;
    unsigned long long uid;   /* owner id (POSIX st_uid; 0 on Windows) */
    unsigned long long gid;   /* group id (POSIX st_gid; 0 on Windows) */
} proven_sys_fs_stat_t;

[[nodiscard]]
bool proven_sys_fs_stat(const char *path, proven_sys_fs_stat_t *out_stat);

[[nodiscard]]
bool proven_sys_fs_link(const char *oldpath, const char *newpath);

[[nodiscard]]
bool proven_sys_fs_symlink(const char *target, const char *linkpath);

// --- Memory Mapping PAL ---
typedef struct {
    void *ptr;
    void *internal_handle; // Map HANDLE for Win32, dummy for POSIX
} proven_sys_mmap_res_t;

/**
 * @param prot 1: Read, 2: Write, 4: Exec
 * @param flags 1: Private, 2: Shared
 */
[[nodiscard]]
proven_sys_mmap_res_t proven_sys_fs_create(proven_sys_file_handle_t handle, size_t offset, size_t size, int prot, int flags);

/**
 * @brief Returns the required file offset granularity for memory mapping on the current platform.
 */
[[nodiscard]]
size_t proven_sys_fs_mmap_offset_granularity(void);

[[nodiscard]]
bool proven_sys_fs_destroy(void *ptr, size_t size, void *internal_handle);

[[nodiscard]]
bool proven_sys_fs_sync(void *ptr, size_t size);

#endif /* PROVEN_PLATFORM_SYS_FS_H */
