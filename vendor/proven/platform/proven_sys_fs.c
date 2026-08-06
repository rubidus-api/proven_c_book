#if !defined(_WIN32) && !defined(_WIN64)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "proven_sys_fs.h"
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#endif

// Internal helper for UTF-8 to Wide conversion on Windows
#if defined(_WIN32) || defined(_WIN64)
static wchar_t *utf8_to_wide_alloc(const char *src) {
    if (!src) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, NULL, 0);
    if (len <= 0) return NULL;
    
    // Allocate temporary buffer for initial conversion
    size_t len_sz = (size_t)len;
    wchar_t *tmp = HeapAlloc(GetProcessHeap(), 0, len_sz * sizeof(wchar_t));
    if (!tmp) return NULL;
    
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, tmp, len) <= 0) {
        HeapFree(GetProcessHeap(), 0, tmp);
        return NULL;
    }

    // Get full path length
    DWORD full_len = GetFullPathNameW(tmp, 0, NULL, NULL);
    if (full_len == 0) {
        HeapFree(GetProcessHeap(), 0, tmp);
        return NULL;
    }

    // Allocate enough including potential \\?\ prefix
    wchar_t *dst = HeapAlloc(GetProcessHeap(), 0, (full_len + 8) * sizeof(wchar_t));
    if (!dst) {
        HeapFree(GetProcessHeap(), 0, tmp);
        return NULL;
    }

    GetFullPathNameW(tmp, full_len, dst, NULL);
    HeapFree(GetProcessHeap(), 0, tmp);

    // If it's a long absolute path and not already prefixed
    if (full_len >= MAX_PATH && wcsncmp(dst, L"\\\\?\\", 4) != 0 && wcsncmp(dst, L"\\\\.\\", 4) != 0) {
        wchar_t *prefixed = HeapAlloc(GetProcessHeap(), 0, (full_len + 8) * sizeof(wchar_t));
        if (prefixed) {
            if (wcsncmp(dst, L"\\\\", 2) == 0) { // UNC path
                lstrcpyW(prefixed, L"\\\\?\\UNC\\");
                lstrcatW(prefixed, dst + 2);
            } else {
                lstrcpyW(prefixed, L"\\\\?\\");
                lstrcatW(prefixed, dst);
            }
            HeapFree(GetProcessHeap(), 0, dst);
            return prefixed;
        }
    }

    return dst;
}
#endif

proven_sys_file_handle_t proven_sys_fs_open(const char *path, int flags) {
    if (!path) {
#if defined(_WIN32) || defined(_WIN64)
        return (proven_sys_file_handle_t){ .handle = NULL };
#else
        return (proven_sys_file_handle_t){ .fd = -1 };
#endif
    }
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wpath = utf8_to_wide_alloc(path);
    if (!wpath) return (proven_sys_file_handle_t){ .handle = NULL };

    DWORD access = 0;
    if (flags & PROVEN_SYS_FS_READ) access |= GENERIC_READ;
    if (flags & PROVEN_SYS_FS_APPEND) {
        access |= FILE_APPEND_DATA;
    } else if (flags & PROVEN_SYS_FS_WRITE) {
        access |= GENERIC_WRITE;
    }
    if ((flags & PROVEN_SYS_FS_TRUNC) && !(flags & PROVEN_SYS_FS_APPEND)) access |= GENERIC_WRITE;

    DWORD disposition = OPEN_EXISTING;
    if (flags & PROVEN_SYS_FS_CREATE_NEW) {
        disposition = CREATE_NEW;
    } else if ((flags & PROVEN_SYS_FS_CREATE) && (flags & PROVEN_SYS_FS_TRUNC)) {
        disposition = CREATE_ALWAYS;
    } else if (flags & PROVEN_SYS_FS_CREATE) {
        disposition = OPEN_ALWAYS;
    } else if (flags & PROVEN_SYS_FS_TRUNC) {
        disposition = TRUNCATE_EXISTING;
    }

    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

    HANDLE h = CreateFileW(wpath, access, share, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    HeapFree(GetProcessHeap(), 0, wpath);
    if (h == INVALID_HANDLE_VALUE) return (proven_sys_file_handle_t){ .handle = NULL };
    
    return (proven_sys_file_handle_t){ .handle = (void*)h };
#else
    int o_flags = 0;
    const bool wants_read = (flags & PROVEN_SYS_FS_READ) != 0;
    const bool wants_write = (flags & (PROVEN_SYS_FS_WRITE | PROVEN_SYS_FS_APPEND)) != 0;
    if (wants_read && wants_write) {
        o_flags = O_RDWR;
    } else if (wants_write) {
        o_flags = O_WRONLY;
    } else {
        o_flags = O_RDONLY;
    }

    if (flags & PROVEN_SYS_FS_APPEND) o_flags |= O_APPEND;
    if (flags & PROVEN_SYS_FS_CREATE_NEW) o_flags |= O_CREAT | O_EXCL;
    else if (flags & PROVEN_SYS_FS_CREATE) o_flags |= O_CREAT;
    
    if (flags & PROVEN_SYS_FS_TRUNC)  o_flags |= O_TRUNC;
    
    int fd = open(path, o_flags, 0666);
    if (fd < 0) return (proven_sys_file_handle_t){ .fd = -1 };
    return (proven_sys_file_handle_t){ .fd = fd };
#endif
}

bool proven_sys_fs_close(proven_sys_file_handle_t handle) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return false;
    return CloseHandle((HANDLE)handle.handle) != 0;
#else
    if (handle.fd < 0) return false;
    /* EINTR on close: on Linux the descriptor is closed anyway, and retrying could close
     * a descriptor another thread has just been given. Treat it as closed. */
    if (close(handle.fd) != 0) {
        return errno == EINTR;
    }
    return true;
#endif
}

proven_sys_result_size_t proven_sys_fs_read(proven_sys_file_handle_t handle, void *buf, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#else
    if (handle.fd < 0) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#endif
    if (size > 0 && !buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };

#if defined(_WIN32) || defined(_WIN64)
    HANDLE h = (HANDLE)handle.handle;
    DWORD to_read = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : (DWORD)size;
    DWORD read_bytes = 0;
    if (ReadFile(h, buf, to_read, &read_bytes, NULL)) {
        if (read_bytes == 0) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
        return (proven_sys_result_size_t){ PROVEN_OK, (size_t)read_bytes };
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_HANDLE_EOF) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
#else
    int fd = handle.fd;
    size_t to_read = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : size;
    ssize_t r;
    do {
        r = read(fd, buf, to_read);
    } while (r < 0 && errno == EINTR);
    
    if (r < 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    if (r == 0) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)r };
#endif
}

proven_sys_result_size_t proven_sys_fs_write(proven_sys_file_handle_t handle, const void *buf, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#else
    if (handle.fd < 0) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#endif
    if (size > 0 && !buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };

#if defined(_WIN32) || defined(_WIN64)
    HANDLE h = (HANDLE)handle.handle;
    DWORD to_write = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : (DWORD)size;
    DWORD written = 0;
    if (WriteFile(h, buf, to_write, &written, NULL)) {
        if (written == 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
        return (proven_sys_result_size_t){ PROVEN_OK, (size_t)written };
    } else {
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
#else
    int fd = handle.fd;
    size_t to_write = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : size;
    ssize_t r;
    do {
        r = write(fd, buf, to_write);
    } while (r < 0 && errno == EINTR);
    
    if (r < 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    if (r == 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)r };
#endif
}

proven_sys_result_size_t proven_sys_fs_size(proven_sys_file_handle_t handle) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    if (GetFileType((HANDLE)handle.handle) != FILE_TYPE_DISK) {
        return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    }
    LARGE_INTEGER li;
    if (GetFileSizeEx((HANDLE)handle.handle, &li)) {
        uint64_t sz = (uint64_t)li.QuadPart;
        if (sz > (uint64_t)PROVEN_SIZE_MAX) return (proven_sys_result_size_t){ PROVEN_ERR_OVERFLOW, 0 };
        return (proven_sys_result_size_t){ PROVEN_OK, (size_t)sz };
    }
    return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
#else
    if (handle.fd < 0) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    struct stat st;
    if (fstat(handle.fd, &st) == 0) {
        if (!S_ISREG(st.st_mode)) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
        if (st.st_size < 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
        if ((uintmax_t)st.st_size > (uintmax_t)PROVEN_SIZE_MAX) return (proven_sys_result_size_t){ PROVEN_ERR_OVERFLOW, 0 };
        return (proven_sys_result_size_t){ PROVEN_OK, (size_t)st.st_size };
    }
    return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
#endif
}

bool proven_sys_fs_rename(const char *src, const char *dest) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wsrc = utf8_to_wide_alloc(src);
    wchar_t *wdest = utf8_to_wide_alloc(dest);
    if (!wsrc || !wdest) {
        if (wsrc) HeapFree(GetProcessHeap(), 0, wsrc);
        if (wdest) HeapFree(GetProcessHeap(), 0, wdest);
        return false;
    }
    bool success = MoveFileW(wsrc, wdest) != 0;
    HeapFree(GetProcessHeap(), 0, wsrc);
    HeapFree(GetProcessHeap(), 0, wdest);
    return success;
#else
    return rename(src, dest) == 0;
#endif
}

bool proven_sys_fs_remove(const char *path) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wpath = utf8_to_wide_alloc(path);
    if (!wpath) return false;
    bool success = DeleteFileW(wpath) != 0;
    HeapFree(GetProcessHeap(), 0, wpath);
    return success;
#else
    return remove(path) == 0;
#endif
}

bool proven_sys_fs_mkdir(const char *path) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wpath = utf8_to_wide_alloc(path);
    if (!wpath) return false;
    bool success = CreateDirectoryW(wpath, NULL) != 0;
    HeapFree(GetProcessHeap(), 0, wpath);
    return success;
#else
    return mkdir(path, 0755) == 0;
#endif
}

bool proven_sys_fs_rmdir(const char *path) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wpath = utf8_to_wide_alloc(path);
    if (!wpath) return false;
    bool success = RemoveDirectoryW(wpath) != 0;
    HeapFree(GetProcessHeap(), 0, wpath);
    return success;
#else
    return rmdir(path) == 0;
#endif
}

proven_sys_dir_handle_t proven_sys_fs_dir_open(const char *path) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wpath_orig = utf8_to_wide_alloc(path);
    if (!wpath_orig) return (proven_sys_dir_handle_t){ .internal = NULL };
    
    size_t len = (size_t)lstrlenW(wpath_orig);
    wchar_t *wpath = HeapAlloc(GetProcessHeap(), 0, (len + 3) * sizeof(wchar_t));
    if (!wpath) {
        HeapFree(GetProcessHeap(), 0, wpath_orig);
        return (proven_sys_dir_handle_t){ .internal = NULL };
    }
    lstrcpyW(wpath, wpath_orig);
    lstrcatW(wpath, L"\\*");
    HeapFree(GetProcessHeap(), 0, wpath_orig);

    WIN32_FIND_DATAW *fd = HeapAlloc(GetProcessHeap(), 0, sizeof(WIN32_FIND_DATAW));
    if (!fd) {
        HeapFree(GetProcessHeap(), 0, wpath);
        return (proven_sys_dir_handle_t){ .internal = NULL };
    }

    HANDLE h = FindFirstFileW(wpath, fd);
    HeapFree(GetProcessHeap(), 0, wpath);
    if (h == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, fd);
        return (proven_sys_dir_handle_t){ .internal = NULL };
    }
    // We store the handle and the first result
    struct win_dir { HANDLE h; WIN32_FIND_DATAW fd; bool first; char *utf8_name; size_t utf8_cap; };
    struct win_dir *wd = HeapAlloc(GetProcessHeap(), 0, sizeof(struct win_dir));
    if (!wd) {
        FindClose(h);
        HeapFree(GetProcessHeap(), 0, fd);
        return (proven_sys_dir_handle_t){ .internal = NULL };
    }
    wd->h = h;
    wd->fd = *fd;
    wd->first = true;
    wd->utf8_name = NULL;
    wd->utf8_cap = 0;
    HeapFree(GetProcessHeap(), 0, fd);
    return (proven_sys_dir_handle_t){ .internal = (void*)wd };
#else
    DIR *d = opendir(path);
    return (proven_sys_dir_handle_t){ .internal = (void*)d };
#endif
}

proven_sys_dir_handle_t proven_sys_fs_dir_open_at(proven_sys_dir_handle_t parent, const char *name) {
#if defined(_WIN32) || defined(_WIN64)
    (void)parent; (void)name;
    return (proven_sys_dir_handle_t){ .internal = NULL };
#else
    if (!parent.internal || !name) return (proven_sys_dir_handle_t){ .internal = NULL };
    DIR *pd = (DIR *)parent.internal;
    int pfd = dirfd(pd);
    if (pfd < 0) return (proven_sys_dir_handle_t){ .internal = NULL };

    /* O_NOFOLLOW is the whole point: if `name` is a symlink now (whatever it was when it
     * was listed), the open fails rather than following it out of the tree. */
    int fd = openat(pfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return (proven_sys_dir_handle_t){ .internal = NULL };

    DIR *d = fdopendir(fd);
    if (!d) { close(fd); return (proven_sys_dir_handle_t){ .internal = NULL }; }
    return (proven_sys_dir_handle_t){ .internal = (void *)d };
#endif
}

bool proven_sys_fs_dir_supports_open_at(void) {
#if defined(_WIN32) || defined(_WIN64)
    return false;
#else
    return true;
#endif
}

bool proven_sys_fs_dir_ids(proven_sys_dir_handle_t handle, unsigned long long *dev, unsigned long long *ino) {
#if defined(_WIN32) || defined(_WIN64)
    (void)handle; (void)dev; (void)ino;
    return false;
#else
    if (!handle.internal) return false;
    DIR *d = (DIR *)handle.internal;
    int fd = dirfd(d);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0) return false;
    if (dev) *dev = (unsigned long long)st.st_dev;
    if (ino) *ino = (unsigned long long)st.st_ino;
    return true;
#endif
}

void proven_sys_fs_dir_close(proven_sys_dir_handle_t handle) {
    if (!handle.internal) return;
#if defined(_WIN32) || defined(_WIN64)
    struct win_dir { HANDLE h; WIN32_FIND_DATAW fd; bool first; char *utf8_name; size_t utf8_cap; } *wd = handle.internal;
    FindClose(wd->h);
    if (wd->utf8_name) HeapFree(GetProcessHeap(), 0, wd->utf8_name);
    HeapFree(GetProcessHeap(), 0, wd);
#else
    closedir((DIR*)handle.internal);
#endif
}

int proven_sys_fs_dir_step(proven_sys_dir_handle_t handle, proven_sys_dir_entry_t *out_entry) {
    if (!handle.internal || !out_entry) return -1;
#if defined(_WIN32) || defined(_WIN64)
    struct win_dir { HANDLE h; WIN32_FIND_DATAW fd; bool first; char *utf8_name; size_t utf8_cap; } *wd = handle.internal;
    if (!wd->first) {
        if (!FindNextFileW(wd->h, &wd->fd)) {
            return (GetLastError() == ERROR_NO_MORE_FILES) ? 0 : -1;
        }
    }
    wd->first = false;

    // Skip . and ..
    while (wd->fd.cFileName[0] == L'.') {
        if (wd->fd.cFileName[1] == L'\0' || (wd->fd.cFileName[1] == L'.' && wd->fd.cFileName[2] == L'\0')) {
            if (!FindNextFileW(wd->h, &wd->fd)) {
                return (GetLastError() == ERROR_NO_MORE_FILES) ? 0 : -1;
            }
            continue;
        }
        break;
    }

    // Convert back from Wide to UTF-8
    int required_size = WideCharToMultiByte(CP_UTF8, 0, wd->fd.cFileName, -1, NULL, 0, NULL, NULL);
    if (required_size <= 0) {
        return -1; // Conversion failed
    } else {
        if (!wd->utf8_name || wd->utf8_cap < (size_t)required_size) {
            if (wd->utf8_name) HeapFree(GetProcessHeap(), 0, wd->utf8_name);
            wd->utf8_name = HeapAlloc(GetProcessHeap(), 0, (size_t)required_size);
            wd->utf8_cap = (size_t)required_size;
        }
        if (wd->utf8_name) {
            int n = WideCharToMultiByte(CP_UTF8, 0, wd->fd.cFileName, -1, wd->utf8_name, required_size, NULL, NULL);
            if (n <= 0) {
                return -1;
            } else {
                out_entry->name = wd->utf8_name;
            }
        } else {
            return -1; // Allocation failed
        }
    }
    out_entry->is_symlink = (wd->fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    out_entry->is_dir = (wd->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    out_entry->is_regular = !out_entry->is_dir && !out_entry->is_symlink;
    uint64_t sz = ((uint64_t)wd->fd.nFileSizeHigh << 32) | wd->fd.nFileSizeLow;
    if (sz > (uint64_t)PROVEN_SIZE_MAX) out_entry->size = PROVEN_SIZE_MAX;
    else out_entry->size = (size_t)sz;
    return 1;
#else
    DIR *d = (DIR*)handle.internal;
    struct dirent *entry;
    /* readdir() returns NULL for both end-of-directory and failure. errno is the only
     * thing that tells them apart, and it is only meaningful if we clear it first. */
    errno = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            if (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')) continue;
        }
        out_entry->name = entry->d_name;
        
        struct stat st;
        /*
         * FOLLOW the symlink, the way stat() does.
         *
         * With AT_SYMLINK_NOFOLLOW a symlink pointing at a perfectly ordinary file came
         * back as "not a regular file", while proven_fs_stat on the same path said FILE -
         * so a caller filtering a listing on `type == FILE` skipped files it could open and
         * read. Two answers to the same question is worse than either answer. A dangling
         * link fails the follow and lands in the fallback below, which reports OTHER: it
         * cannot be opened, so calling it a file would be the lie that started this.
         */
        struct stat lst;
        out_entry->is_symlink = (fstatat(dirfd(d), entry->d_name, &lst, AT_SYMLINK_NOFOLLOW) == 0) &&
                                S_ISLNK(lst.st_mode);

        if (fstatat(dirfd(d), entry->d_name, &st, 0) == 0) {
            out_entry->is_dir = S_ISDIR(st.st_mode);
            out_entry->is_regular = S_ISREG(st.st_mode) != 0;
            if (S_ISREG(st.st_mode)) {
                if (st.st_size < 0) out_entry->size = 0;
                else if ((uintmax_t)st.st_size > (uintmax_t)PROVEN_SIZE_MAX) out_entry->size = PROVEN_SIZE_MAX;
                else out_entry->size = (size_t)st.st_size;
            } else {
                out_entry->size = 0;
            }
        } else {
            out_entry->is_dir = false;      // Fallback: we do not know what it is,
            out_entry->is_regular = false;  // so we must not claim it is a file.
            out_entry->size = 0;
        }
        return 1;
    }
    return (errno != 0) ? -1 : 0;
#endif
}

bool proven_sys_fs_dir_next(proven_sys_dir_handle_t handle, proven_sys_dir_entry_t *out_entry) {
    return proven_sys_fs_dir_step(handle, out_entry) == 1;
}

bool proven_sys_fs_chmod(const char *path, unsigned int perms) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wpath = utf8_to_wide_alloc(path);
    if (!wpath) return false;
    DWORD attr = GetFileAttributesW(wpath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        HeapFree(GetProcessHeap(), 0, wpath);
        return false;
    }
    
    // Logic: If Owner-W (bit 7) is missing, set ReadOnly
    if (!(perms & 0200)) attr |= FILE_ATTRIBUTE_READONLY;
    else attr &= (DWORD)~((DWORD)FILE_ATTRIBUTE_READONLY);
    
    bool success = SetFileAttributesW(wpath, attr) != 0;
    HeapFree(GetProcessHeap(), 0, wpath);
    return success;
#else
    return chmod(path, perms) == 0;
#endif
}

bool proven_sys_fs_lock(proven_sys_file_handle_t handle, int type, bool wait) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return false;
    HANDLE h = (HANDLE)handle.handle;
    OVERLAPPED ov = {0};
    DWORD flags = (type == 1) ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    if (!wait) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    
    if (type == 2) return UnlockFileEx(h, 0, 0xFFFFFFFF, 0xFFFFFFFF, &ov) != 0;
    return LockFileEx(h, flags, 0, 0xFFFFFFFF, 0xFFFFFFFF, &ov) != 0;
#else
    if (handle.fd < 0) return false;
    int fd = handle.fd;
    struct flock fl = {0};
    if (type == 0) fl.l_type = F_RDLCK;
    else if (type == 1) fl.l_type = F_WRLCK;
    else fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    int cmd = wait ? F_SETLKW : F_SETLK;
    return fcntl(fd, cmd, &fl) == 0;
#endif
}

bool proven_sys_fs_stat(const char *path, proven_sys_fs_stat_t *out_stat) {
    if (!path || !out_stat) return false;
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wpath = utf8_to_wide_alloc(path);
    if (!wpath) return false;
    
    // We use CreateFileW + GetFileInformationByHandle to get Volume/File ID for identity
    // FILE_FLAG_BACKUP_SEMANTICS is required to open directories
    HANDLE h = CreateFileW(wpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HeapFree(GetProcessHeap(), 0, wpath);
    if (h == INVALID_HANDLE_VALUE) return false;

    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(h, &info)) {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);

    out_stat->is_dir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    out_stat->is_regular = !out_stat->is_dir &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    if (out_stat->is_dir) {
        out_stat->size = 0;
    } else {
        uint64_t sz = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;
        if (sz > (uint64_t)PROVEN_SIZE_MAX) return false;
        out_stat->size = (size_t)sz;
    }
    out_stat->mode = out_stat->is_dir ? 0755u : 0644u;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) out_stat->mode &= (unsigned int)~0222u;
    
    // Convert FILETIME to Unix timestamp. Pre-1970 values are represented
    // as negative seconds instead of underflowing the unsigned subtraction.
    ULARGE_INTEGER ull;
    ull.LowPart = info.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = info.ftLastWriteTime.dwHighDateTime;
    if (ull.QuadPart < 116444736000000000ULL) {
        out_stat->mtime = -(long long)((116444736000000000ULL - ull.QuadPart + 9999999ULL) / 10000000ULL);
    } else {
        out_stat->mtime = (long long)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
    }

    out_stat->dev = (unsigned long long)info.dwVolumeSerialNumber;
    out_stat->ino = ((unsigned long long)info.nFileIndexHigh << 32) | (unsigned long long)info.nFileIndexLow;
    out_stat->uid = 0;   /* Windows has no POSIX uid/gid; see proven_fs_stat_t docs */
    out_stat->gid = 0;
    return true;
#else
    struct stat st;
    if (stat(path, &st) != 0) return false;
    
    if (S_ISREG(st.st_mode)) {
        if (st.st_size < 0) return false;
        if ((uintmax_t)st.st_size > (uintmax_t)PROVEN_SIZE_MAX) return false;
        out_stat->size = (size_t)st.st_size;
    } else {
        out_stat->size = 0;
    }
    
    out_stat->is_dir = S_ISDIR(st.st_mode);
    out_stat->is_regular = S_ISREG(st.st_mode) != 0;
    out_stat->mode = (unsigned int)st.st_mode;
    out_stat->mtime = (long long)st.st_mtime;
    out_stat->dev = (unsigned long long)st.st_dev;
    out_stat->ino = (unsigned long long)st.st_ino;
    out_stat->uid = (unsigned long long)st.st_uid;
    out_stat->gid = (unsigned long long)st.st_gid;
    return true;
#endif
}

bool proven_sys_fs_link(const char *oldpath, const char *newpath) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wold = utf8_to_wide_alloc(oldpath);
    wchar_t *wnew = utf8_to_wide_alloc(newpath);
    if (!wold || !wnew) {
        if (wold) HeapFree(GetProcessHeap(), 0, wold);
        if (wnew) HeapFree(GetProcessHeap(), 0, wnew);
        return false;
    }
    bool success = CreateHardLinkW(wnew, wold, NULL) != 0;
    HeapFree(GetProcessHeap(), 0, wold);
    HeapFree(GetProcessHeap(), 0, wnew);
    return success;
#else
    return link(oldpath, newpath) == 0;
#endif
}

bool proven_sys_fs_symlink(const char *target, const char *linkpath) {
#if defined(_WIN32) || defined(_WIN64)
    wchar_t *wtarget = utf8_to_wide_alloc(target);
    wchar_t *wlink = utf8_to_wide_alloc(linkpath);
    if (!wtarget || !wlink) {
        if (wtarget) HeapFree(GetProcessHeap(), 0, wtarget);
        if (wlink) HeapFree(GetProcessHeap(), 0, wlink);
        return false;
    }
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    // Note: requires checking if target is a directory for the flag
    bool success = CreateSymbolicLinkW(wlink, wtarget, flags) != 0;
    HeapFree(GetProcessHeap(), 0, wtarget);
    HeapFree(GetProcessHeap(), 0, wlink);
    return success;
#else
    return symlink(target, linkpath) == 0;
#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/mman.h>
#endif

proven_sys_mmap_res_t proven_sys_fs_create(proven_sys_file_handle_t handle, size_t offset, size_t size, int prot, int flags) {
    proven_sys_mmap_res_t res = { .ptr = NULL, .internal_handle = NULL };
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return res;
    HANDLE hFile = (HANDLE)handle.handle;
    DWORD flProtect = 0;
    DWORD dwAccess = 0;

    if (prot & 0x02) { // Write
        flProtect = PAGE_READWRITE;
        dwAccess = FILE_MAP_WRITE;
    } else {
        flProtect = PAGE_READONLY;
        dwAccess = FILE_MAP_READ;
    }

    if (flags & 0x01) { // Private (Copy on write)
        flProtect = PAGE_WRITECOPY;
        dwAccess = FILE_MAP_COPY;
    }

    // Windows requires a separate Mapping object
    HANDLE hMap = CreateFileMappingW(hFile, NULL, flProtect, 0, 0, NULL);
    if (!hMap) return res;

    uint64_t map_offset = (uint64_t)offset;
    void *ptr = MapViewOfFile(hMap, dwAccess,
                              (DWORD)(map_offset >> 32),
                              (DWORD)(map_offset & UINT64_C(0xFFFFFFFF)),
                              size);
    if (!ptr) {
        CloseHandle(hMap);
        return res;
    }

    res.ptr = ptr;
    res.internal_handle = (void*)hMap;
    return res;
#else
    if (handle.fd < 0) return res;
    int fd = handle.fd;
    int p = 0;
    if (prot & 0x01) p |= PROT_READ;
    if (prot & 0x02) p |= PROT_WRITE;
    if (prot & 0x04) p |= PROT_EXEC;

    int f = 0;
    if (flags & 0x01) f |= MAP_PRIVATE;
    if (flags & 0x02) f |= MAP_SHARED;

    off_t mmap_offset = (off_t)offset;
    if ((size_t)mmap_offset != offset) return res;

    void *ptr = mmap(NULL, size, p, f, fd, mmap_offset);
    if (ptr == MAP_FAILED) return res;

    res.ptr = ptr;
    return res;
#endif
}

size_t proven_sys_fs_mmap_offset_granularity(void) {
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (size_t)info.dwAllocationGranularity;
#else
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }
    return (size_t)page_size;
#endif
}

bool proven_sys_fs_destroy(void *ptr, size_t size, void *internal_handle) {
#if defined(_WIN32) || defined(_WIN64)
    (void)size;
    UnmapViewOfFile(ptr);
    if (internal_handle) CloseHandle((HANDLE)internal_handle);
    return true;
#else
    (void)internal_handle;
    return munmap(ptr, size) == 0;
#endif
}

bool proven_sys_fs_sync(void *ptr, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    return FlushViewOfFile(ptr, size) != 0;
#else
    return msync(ptr, size, MS_SYNC) == 0;
#endif
}
