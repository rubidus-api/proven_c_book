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
    PROVEN_SYS_FS_CREATE_NEW = 1 << 5,
    /**
     * Create the file owner-only (POSIX mode 0600) instead of the default 0666 & ~umask.
     *
     * Only meaningful together with CREATE or CREATE_NEW, and only for a file this call
     * actually creates: an existing file keeps the mode it already has. It exists because
     * narrowing permissions after creation does not close the window - a descriptor another
     * user opened during it stays open and stays readable. The mode has to be on the
     * creating syscall itself.
     *
     * On Windows this is currently a no-op: confidentiality there is an ACL question, and
     * the default DACL is inherited from the parent directory. Do not read a POSIX
     * guarantee into the Windows branch.
     */
    PROVEN_SYS_FS_PRIVATE = 1 << 6
} proven_sys_fs_mode_t;

/** @brief Why an open did not happen. The three a caller can actually act on, and everything else. */
typedef enum {
    PROVEN_SYS_FS_OPEN_OK = 0,
    PROVEN_SYS_FS_OPEN_NOT_FOUND,  /**< the name is not there */
    PROVEN_SYS_FS_OPEN_DENIED,     /**< permission refused it */
    PROVEN_SYS_FS_OPEN_BUSY,       /**< something else holds it right now */
    PROVEN_SYS_FS_OPEN_ERROR       /**< anything else, including an exclusive-create collision */
} proven_sys_fs_open_result_t;

/**
 * @brief Open, saying WHY when it fails.
 *
 * "Not there", "you may not", and "not right now" are three different problems with three
 * different answers, and a caller told only that something went wrong can act on none of
 * them. `out_reason` may be NULL.
 */
[[nodiscard]]
proven_sys_file_handle_t proven_sys_fs_open_checked(const char *path, int flags,
                                                    proven_sys_fs_open_result_t *out_reason);

/** @brief Convenience wrapper over proven_sys_fs_open_checked; it cannot say why. */
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

/** @brief Why a rename did not happen. "No" has more than one meaning and callers need them apart. */
typedef enum {
    PROVEN_SYS_FS_RENAME_OK = 0,
    PROVEN_SYS_FS_RENAME_DENIED,   /**< permission refused it: the target is protected */
    PROVEN_SYS_FS_RENAME_BUSY,     /**< something else holds it open right now */
    PROVEN_SYS_FS_RENAME_ERROR     /**< anything else */
} proven_sys_fs_rename_result_t;

/**
 * @brief Rename, saying WHY when it fails.
 *
 * A single boolean collapses "you may not" and "not right now" and "the disk is broken"
 * into one answer, and a caller can act on all three differently: ask the user, retry,
 * give up. The two the platforms actually produce here are a protected destination
 * (a read-only file on Windows, a directory without write permission on POSIX) and a
 * destination another process is holding open.
 */
[[nodiscard]]
proven_sys_fs_rename_result_t proven_sys_fs_rename_checked(const char *src, const char *dest);

/**
 * @brief Rename `src` to `dest`, REPLACING `dest` if it already exists.
 *
 * Replacement is required, not incidental: the whole-file atomic writes rename a staging
 * file over their target, and a rename that refuses an existing destination makes the
 * second write to any name fail.
 *
 * Same volume only. On Windows this is MoveFileExW with MOVEFILE_REPLACE_EXISTING and
 * without MOVEFILE_COPY_ALLOWED, so a cross-volume move fails rather than silently
 * degrading into a copy-and-delete that is not atomic. The destination is never deleted
 * first: that would open an interval in which the name does not exist.
 *
 * Not established by any test result here: the behaviour against a read-only destination,
 * ACL and metadata handling, sharing modes, and symlinks. Those are native Windows
 * questions and this workstation has never run a Windows binary.
 */
[[nodiscard]]
bool proven_sys_fs_rename(const char *src, const char *dest);
/* ^ convenience wrapper over proven_sys_fs_rename_checked; it cannot say why. */

/**
 * @brief Remove a name, saying WHY when it fails.
 *
 * Deleting is a DIRECTORY operation, and the two platforms disagree about whether the
 * file's own mode has a say: POSIX unlink never consulted it, Windows DeleteFile refuses a
 * read-only file outright. The library does not paper over that - it reports it, so a
 * caller sees PROVEN_ERR_PERMISSION rather than a bare I/O error and can clear the mark.
 */
[[nodiscard]]
proven_sys_fs_open_result_t proven_sys_fs_remove_checked(const char *path);

/** @brief Convenience wrapper over proven_sys_fs_remove_checked; it cannot say why. */
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

/**
 * @brief Set permissions through an OPEN HANDLE rather than by pathname.
 *
 * The pathname form has to resolve the name again, and between the creation and that
 * second resolution the name can come to mean a different file. The handle cannot: it
 * refers to the object that was opened and to nothing else.
 *
 * On Windows this maps the owner-write bit onto the read-only attribute, exactly as the
 * pathname form does. Returns false if the OS reported an error.
 */
[[nodiscard]]
bool proven_sys_fs_fchmod(proven_sys_file_handle_t handle, unsigned int perms);

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

/** @brief What a metadata lookup found: the file, nothing, or an error that is not "nothing". */
typedef enum {
    PROVEN_SYS_FS_STAT_OK = 0,
    PROVEN_SYS_FS_STAT_NOT_FOUND,   /**< the name does not exist (ENOENT/ENOTDIR) */
    PROVEN_SYS_FS_STAT_ERROR        /**< the lookup itself failed: permission, I/O, a bad handle */
} proven_sys_fs_stat_result_t;

/**
 * @brief Metadata lookup that distinguishes "no such file" from "could not tell".
 *
 * A boolean answer collapses the two, and a caller that is about to write private bytes
 * needs them apart: a missing target means there is no mode to carry across, while a
 * failed lookup means the caller does not know what it is about to overwrite and must
 * not guess.
 */
[[nodiscard]]
proven_sys_fs_stat_result_t proven_sys_fs_stat_checked(const char *path, proven_sys_fs_stat_t *out_stat);

/** @brief Convenience wrapper: metadata was retrieved. Cannot tell missing from failed. */
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
