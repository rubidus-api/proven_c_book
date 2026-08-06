#ifndef PROVEN_PLATFORM_SYS_IO_H
#define PROVEN_PLATFORM_SYS_IO_H

#include "proven/types.h"
#include "proven/error.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @file proven_sys_io.h
 * @brief Platform Abstraction Layer specifically for Standard Console I/O.
 *        This eliminates the need for <stdio.h> via raw syscalls where possible,
 *        adhering strictly to isolated OS/Hardware constraints.
 */

typedef struct {
    union {
        void *handle;
        int fd;
    };
} proven_sys_io_handle_t;

[[nodiscard]] proven_sys_io_handle_t proven_sys_io_std_in(void);
[[nodiscard]] proven_sys_io_handle_t proven_sys_io_std_out(void);
[[nodiscard]] proven_sys_io_handle_t proven_sys_io_std_err(void);

typedef struct {
    proven_err_t err;
    proven_size_t value;
} proven_sys_result_size_t;

[[nodiscard]]
proven_sys_result_size_t proven_sys_io_write_once(proven_sys_io_handle_t handle, const void *buf, size_t size);

[[nodiscard]]
proven_sys_result_size_t proven_sys_io_write_all(proven_sys_io_handle_t handle, const void *buf, size_t size);

[[nodiscard]]
proven_sys_result_size_t proven_sys_io_read_once(proven_sys_io_handle_t handle, void *buf, size_t size);

[[nodiscard]]
proven_sys_result_size_t proven_sys_io_read_all(proven_sys_io_handle_t handle, void *buf, size_t size);

/**
 * @brief Does nothing. There is no buffer anywhere in this library.
 * @note Kept only because callers exist. Use proven_sys_io_sync() for durability.
 */
void proven_sys_io_flush(proven_sys_io_handle_t handle);

typedef struct {
    proven_err_t err;
    uint64_t     value;
} proven_sys_result_u64_t;

#define PROVEN_SYS_IO_SEEK_SET 0
#define PROVEN_SYS_IO_SEEK_CUR 1
#define PROVEN_SYS_IO_SEEK_END 2

/**
 * @brief Force this handle's data to the storage device (fsync / FlushFileBuffers).
 */
[[nodiscard]]
proven_err_t proven_sys_io_sync(proven_sys_io_handle_t handle);

/**
 * @brief Move the file position. Returns the resulting absolute offset.
 * @note A handle that cannot seek (a pipe, a terminal) returns PROVEN_ERR_UNSUPPORTED,
 *       not PROVEN_ERR_IO: not seekable is a property of the thing, not a failure.
 */
[[nodiscard]]
proven_sys_result_u64_t proven_sys_io_seek(proven_sys_io_handle_t handle, int64_t offset, int whence);

[[nodiscard]]
proven_sys_result_size_t proven_sys_io_seek_relative(proven_sys_io_handle_t handle, int64_t offset);

[[nodiscard]]
proven_err_t proven_sys_io_truncate(proven_sys_io_handle_t handle, uint64_t length);

/** @brief Positional read. Does not move the file position. */
[[nodiscard]]
proven_sys_result_size_t proven_sys_io_pread(proven_sys_io_handle_t handle, void *buf, size_t size, uint64_t offset);

/** @brief Positional write. Does not move the file position. */
[[nodiscard]]
proven_sys_result_size_t proven_sys_io_pwrite(proven_sys_io_handle_t handle, const void *buf, size_t size, uint64_t offset);

#endif /* PROVEN_PLATFORM_SYS_IO_H */
