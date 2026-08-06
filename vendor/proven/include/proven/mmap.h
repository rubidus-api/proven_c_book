#ifndef PROVEN_MMAP_H
#define PROVEN_MMAP_H

#include "proven/types.h"
#include "proven/u8str.h"
#include "proven/error.h"
#include "proven/fs.h"

/**
 * @brief Memory mapping protection modes.
 */
typedef enum {
    PROVEN_MMAP_READ  = 0x01,
    PROVEN_MMAP_WRITE = 0x02,
    PROVEN_MMAP_EXEC  = 0x04, // Use with caution
} proven_mmap_prot_t;

/**
 * @brief Memory mapping flags.
 */
typedef enum {
    PROVEN_MMAP_PRIVATE = 0x01, // Copy-on-write
    PROVEN_MMAP_SHARED  = 0x02, // Changes visible to other processes
} proven_mmap_flags_t;

/**
 * @brief Represents a memory-mapped file region.
 */
typedef struct {
    void *ptr;               ///< Pointer to the mapped memory
    proven_size_t size;      ///< Size of the mapping
    proven_fs_handle_t file; ///< Internal file handle
    void *internal_handle;   ///< Internal mapping handle (Win32 specific)
    proven_mmap_flags_t flags; ///< PRIVATE or SHARED; sync needs to know which
} proven_mmap_t;

/**
 * @brief Result type for mmap operations.
 */
typedef struct {
    proven_err_t err;
    proven_mmap_t value;
} proven_result_mmap_t;

/**
 * @brief Maps a file into memory.
 * @param file The opened file handle.
 * @param offset Start offset in the file; it must match the platform mapping granularity.
 * @param size Number of bytes to map. 0 means map until EOF.
 * @param prot Protection mode (read/write/exec where supported).
 * @param flags Mapping flags (shared/private).
 */
[[nodiscard]]
proven_result_mmap_t proven_mmap_create(proven_fs_handle_t file, proven_size_t offset, proven_size_t size, proven_mmap_prot_t prot, proven_mmap_flags_t flags);

/**
 * @brief Unmaps the memory region.
 */
[[nodiscard]]
proven_err_t proven_mmap_destroy(proven_mmap_t *mmap);

/**
 * @brief Synchronizes changes back to disk (SHARED mappings only).
 *
 * @note A PRIVATE (copy-on-write) mapping returns PROVEN_ERR_UNSUPPORTED: its writes exist
 *       only in this process and there is nothing to write back. It used to return
 *       PROVEN_OK and persist nothing..
 */
[[nodiscard]]
proven_err_t proven_mmap_sync(proven_mmap_t *mmap);

/**
 * @brief Returns a read-only view of the entire mapped memory.
 */
static inline proven_u8str_view_t proven_mmap_as_view(proven_mmap_t mmap) {
    return (proven_u8str_view_t){ .ptr = (const proven_byte_t*)mmap.ptr, .size = mmap.size };
}

#endif /* PROVEN_MMAP_H */
