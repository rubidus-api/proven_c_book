#include "proven_sys_io.h"

/*
 * Two platforms: Windows, and POSIX. That is all.
 *
 * This file used to implement read, write and seek in hand-written inline
 * assembly, one raw-syscall path per architecture: x86_64 (`syscall`), i386
 * (`int $0x80`), aarch64 (`svc #0`), plus an opt-in ARM32 path. It bought
 * nothing. proven_sys_fs.c, in the same library, already calls libc's open, read,
 * write, close and mmap - libc was always linked and always doing file I/O, so the
 * assembly gained independence from nothing.
 *
 * What it cost was real:
 *
 *   - Three of the four architecture paths had no verification on any machine
 *     without the cross-toolchains, which is most machines.
 *   - Because the console path issued raw `syscall` instructions, an LD_PRELOAD
 *     interposer counted ZERO of proven_println's ten thousand writes. Standard
 *     tracing tooling was blind to this library's console I/O.
 *   - The aarch64 seek used `"=r"(x0)` as a write-only output while also naming x0
 *     as an input - the weaker idiom, and untested.
 *
 * Removing it changes no behaviour: forcing every branch into the POSIX fallback
 * and running the I/O suite passed 12 of 12, byte-identical.
 *
 * See docs/RFC-0001-streams-and-io.md.
 */

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

proven_sys_io_handle_t proven_sys_io_std_in(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (proven_sys_io_handle_t){ .handle = GetStdHandle(STD_INPUT_HANDLE) };
#else
    return (proven_sys_io_handle_t){ .fd = 0 };
#endif
}

proven_sys_io_handle_t proven_sys_io_std_out(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (proven_sys_io_handle_t){ .handle = GetStdHandle(STD_OUTPUT_HANDLE) };
#else
    return (proven_sys_io_handle_t){ .fd = 1 };
#endif
}

proven_sys_io_handle_t proven_sys_io_std_err(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (proven_sys_io_handle_t){ .handle = GetStdHandle(STD_ERROR_HANDLE) };
#else
    return (proven_sys_io_handle_t){ .fd = 2 };
#endif
}

proven_sys_result_size_t proven_sys_io_write_once(proven_sys_io_handle_t handle, const void *buf, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#else
    if (handle.fd < 0) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#endif
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

#if defined(_WIN32) || defined(_WIN64)
    DWORD to_write = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : (DWORD)size;
    DWORD written = 0;
    if (!WriteFile((HANDLE)handle.handle, buf, to_write, &written, NULL)) {
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
    if (written == 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)written };
#else
    ssize_t ret;
    do {
        ret = write(handle.fd, buf, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    if (ret == 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)ret };
#endif
}

proven_sys_result_size_t proven_sys_io_write_all(proven_sys_io_handle_t handle, const void *buf, size_t size) {
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

    size_t total_written = 0;
    while (total_written < size) {
        proven_sys_result_size_t res =
            proven_sys_io_write_once(handle, (const unsigned char *)buf + total_written, size - total_written);
        if (res.err != PROVEN_OK) return res;
        total_written += res.value;
    }
    return (proven_sys_result_size_t){ PROVEN_OK, total_written };
}

proven_sys_result_size_t proven_sys_io_read_once(proven_sys_io_handle_t handle, void *buf, size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#else
    if (handle.fd < 0) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
#endif
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

#if defined(_WIN32) || defined(_WIN64)
    DWORD to_read = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : (DWORD)size;
    DWORD read_bytes = 0;
    if (!ReadFile((HANDLE)handle.handle, buf, to_read, &read_bytes, NULL)) {
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
    if (read_bytes == 0) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)read_bytes };
#else
    ssize_t ret;
    do {
        ret = read(handle.fd, (unsigned char *)buf, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    if (ret == 0) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)ret };
#endif
}

proven_sys_result_size_t proven_sys_io_read_all(proven_sys_io_handle_t handle, void *buf, size_t size) {
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

    size_t total_read = 0;
    while (total_read < size) {
        proven_sys_result_size_t res =
            proven_sys_io_read_once(handle, (unsigned char *)buf + total_read, size - total_read);
        if (res.err == PROVEN_ERR_EOF) {
            /* End of input before the request was satisfied: report how far we got. */
            return (proven_sys_result_size_t){ PROVEN_ERR_EOF, total_read };
        }
        if (res.err != PROVEN_OK) return res;
        total_read += res.value;
    }
    return (proven_sys_result_size_t){ PROVEN_OK, total_read };
}

void proven_sys_io_flush(proven_sys_io_handle_t handle) {
    /*
     * Deliberately does nothing, on every platform.
     *
     * Nothing in this library buffers - writes go straight to the OS - so there is
     * nothing to flush. This used to call FlushFileBuffers on Windows, which is a
     * *disk sync*: the same call meant nothing on POSIX and something expensive on
     * Windows, and neither of those is what the word "flush" promised.
     *
     * Durability is now its own explicit call, proven_sys_io_sync(), so a caller who
     * wants the disk to have the bytes has to say so, and pays for it knowingly.
     */
    (void)handle;
}

proven_err_t proven_sys_io_sync(proven_sys_io_handle_t handle) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return PROVEN_ERR_INVALID_ARG;
    if (!FlushFileBuffers((HANDLE)handle.handle)) return PROVEN_ERR_IO;
    return PROVEN_OK;
#else
    if (handle.fd < 0) return PROVEN_ERR_INVALID_ARG;
    int ret;
    do {
        ret = fsync(handle.fd);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) return PROVEN_ERR_IO;
    return PROVEN_OK;
#endif
}

proven_sys_result_u64_t proven_sys_io_seek(proven_sys_io_handle_t handle, int64_t offset, int whence) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_u64_t){ PROVEN_ERR_INVALID_ARG, 0 };
    DWORD method;
    switch (whence) {
        case PROVEN_SYS_IO_SEEK_SET: method = FILE_BEGIN;   break;
        case PROVEN_SYS_IO_SEEK_CUR: method = FILE_CURRENT; break;
        case PROVEN_SYS_IO_SEEK_END: method = FILE_END;     break;
        default: return (proven_sys_result_u64_t){ PROVEN_ERR_INVALID_ARG, 0 };
    }
    LARGE_INTEGER distance;
    distance.QuadPart = offset;
    LARGE_INTEGER result;
    if (!SetFilePointerEx((HANDLE)handle.handle, distance, &result, method)) {
        return (proven_sys_result_u64_t){ PROVEN_ERR_IO, 0 };
    }
    return (proven_sys_result_u64_t){ PROVEN_OK, (uint64_t)result.QuadPart };
#else
    if (handle.fd < 0) return (proven_sys_result_u64_t){ PROVEN_ERR_INVALID_ARG, 0 };
    int w;
    switch (whence) {
        case PROVEN_SYS_IO_SEEK_SET: w = SEEK_SET; break;
        case PROVEN_SYS_IO_SEEK_CUR: w = SEEK_CUR; break;
        case PROVEN_SYS_IO_SEEK_END: w = SEEK_END; break;
        default: return (proven_sys_result_u64_t){ PROVEN_ERR_INVALID_ARG, 0 };
    }
    off_t ret = lseek(handle.fd, (off_t)offset, w);
    if (ret < 0) {
        /* A pipe or a terminal cannot seek. That is not an I/O failure - it is a
         * property of the thing, and callers key off it: the scanner probes with a
         * zero-offset seek to decide whether it is allowed to roll back. */
        if (errno == ESPIPE) return (proven_sys_result_u64_t){ PROVEN_ERR_UNSUPPORTED, 0 };
        return (proven_sys_result_u64_t){ PROVEN_ERR_IO, 0 };
    }
    return (proven_sys_result_u64_t){ PROVEN_OK, (uint64_t)ret };
#endif
}

proven_sys_result_size_t proven_sys_io_seek_relative(proven_sys_io_handle_t handle, int64_t offset) {
    proven_sys_result_u64_t r = proven_sys_io_seek(handle, offset, PROVEN_SYS_IO_SEEK_CUR);
    return (proven_sys_result_size_t){ r.err, (proven_size_t)r.value };
}

proven_err_t proven_sys_io_truncate(proven_sys_io_handle_t handle, uint64_t length) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return PROVEN_ERR_INVALID_ARG;
    /* Windows truncates at the current pointer, so it has to be moved. Put it back
     * afterwards: truncating a file should not silently reposition it for whoever
     * writes next. */
    LARGE_INTEGER saved;
    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    if (!SetFilePointerEx((HANDLE)handle.handle, zero, &saved, FILE_CURRENT)) return PROVEN_ERR_IO;

    LARGE_INTEGER target;
    target.QuadPart = (LONGLONG)length;
    if (!SetFilePointerEx((HANDLE)handle.handle, target, NULL, FILE_BEGIN)) return PROVEN_ERR_IO;
    if (!SetEndOfFile((HANDLE)handle.handle)) {
        (void)SetFilePointerEx((HANDLE)handle.handle, saved, NULL, FILE_BEGIN);
        return PROVEN_ERR_IO;
    }
    if (!SetFilePointerEx((HANDLE)handle.handle, saved, NULL, FILE_BEGIN)) return PROVEN_ERR_IO;
    return PROVEN_OK;
#else
    if (handle.fd < 0) return PROVEN_ERR_INVALID_ARG;
    int ret;
    do {
        ret = ftruncate(handle.fd, (off_t)length);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) return PROVEN_ERR_IO;
    return PROVEN_OK;
#endif
}

proven_sys_result_size_t proven_sys_io_pread(proven_sys_io_handle_t handle, void *buf, size_t size, uint64_t offset) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(offset & 0xFFFFFFFFu);
    ov.OffsetHigh = (DWORD)(offset >> 32);
    DWORD to_read = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : (DWORD)size;
    DWORD got = 0;
    if (!ReadFile((HANDLE)handle.handle, buf, to_read, &got, &ov)) {
        if (GetLastError() == ERROR_HANDLE_EOF) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
    if (got == 0) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)got };
#else
    if (handle.fd < 0) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

    ssize_t ret;
    do {
        ret = pread(handle.fd, buf, size, (off_t)offset);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        if (errno == ESPIPE) return (proven_sys_result_size_t){ PROVEN_ERR_UNSUPPORTED, 0 };
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
    if (ret == 0) return (proven_sys_result_size_t){ PROVEN_ERR_EOF, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)ret };
#endif
}

proven_sys_result_size_t proven_sys_io_pwrite(proven_sys_io_handle_t handle, const void *buf, size_t size, uint64_t offset) {
#if defined(_WIN32) || defined(_WIN64)
    if (!handle.handle) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(offset & 0xFFFFFFFFu);
    ov.OffsetHigh = (DWORD)(offset >> 32);
    DWORD to_write = (size > 0x7FFFFFFF) ? 0x7FFFFFFF : (DWORD)size;
    DWORD put = 0;
    if (!WriteFile((HANDLE)handle.handle, buf, to_write, &put, &ov)) {
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
    if (put == 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)put };
#else
    if (handle.fd < 0) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };
    if (size == 0) return (proven_sys_result_size_t){ PROVEN_OK, 0 };
    if (!buf) return (proven_sys_result_size_t){ PROVEN_ERR_INVALID_ARG, 0 };

    ssize_t ret;
    do {
        ret = pwrite(handle.fd, buf, size, (off_t)offset);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        if (errno == ESPIPE) return (proven_sys_result_size_t){ PROVEN_ERR_UNSUPPORTED, 0 };
        return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    }
    if (ret == 0) return (proven_sys_result_size_t){ PROVEN_ERR_IO, 0 };
    return (proven_sys_result_size_t){ PROVEN_OK, (size_t)ret };
#endif
}
