#ifndef PROVEN_HASH_LEGACY_H
#define PROVEN_HASH_LEGACY_H

#include "proven/types.h"
#include "proven/memory.h"

/**
 * @file hash_legacy.h
 * @brief SHA-1 and MD5: two digests that are broken, and still written into formats.
 *
 * Both have practical collisions. Someone who chooses the input can make two different
 * messages with the same MD5 in seconds and the same SHA-1 for a known, affordable cost. So
 * neither can answer "are these the same content" against a party who wants to fool you -
 * that is `proven_sha256` in hash.h, and nothing in this header replaces it.
 *
 * They are here because other people's formats name them and will not change: the WebSocket
 * handshake (RFC 6455) derives its accept key with SHA-1, HTTP Digest authentication (RFC 7616)
 * still meets MD5 servers, and checksum files published years ago are `md5sum` and `sha1sum`
 * output. When a format you do not control says "SHA-1" or "MD5", this is the header. When you
 * are choosing, it is not.
 *
 * The two families have the same shape as SHA-256: a caller-owned context, init / update /
 * final, a one-shot call and a hex spelling. Nothing allocates and nothing touches the OS, so
 * the header is available in freestanding builds.
 */

// -----------------------------------------------------------------------------
// SHA-1 (FIPS 180-4)
// -----------------------------------------------------------------------------

/** @brief Bytes in a SHA-1 digest. */
#define PROVEN_SHA1_SIZE ((proven_size_t)20)

/**
 * @brief A SHA-1 hashing context. Opaque; use the init/update/final calls.
 *
 * Its fields are exposed only so it can live on the stack with no allocation. Do not read or
 * write them - the layout is not part of the contract.
 */
typedef struct {
    proven_u32  state[5];
    proven_u64  length;      /* total bytes fed, for the length padding */
    proven_byte_t block[64]; /* the partial 64-byte block not yet compressed */
    proven_size_t block_len;
} proven_sha1_t;

/** @brief Begin a SHA-1 digest. */
void proven_sha1_init(proven_sha1_t *ctx);

/**
 * @brief Feed more bytes into a digest in progress. The result depends only on the
 *        concatenation of everything fed, not on how it was split.
 */
void proven_sha1_update(proven_sha1_t *ctx, proven_mem_view_t data);

/**
 * @brief Finish the digest, writing 20 bytes to `out`. The context is spent afterwards;
 *        re-init it to hash something else.
 */
void proven_sha1_final(proven_sha1_t *ctx, proven_byte_t out[PROVEN_SHA1_SIZE]);

/**
 * @brief One-shot SHA-1 of a single buffer. Equivalent to init + one update + final.
 *
 * @warning Not collision resistant. Use it only where a format fixes SHA-1; fingerprint
 *          content with `proven_sha256`.
 */
void proven_sha1(proven_mem_view_t data, proven_byte_t out[PROVEN_SHA1_SIZE]);

/** @brief Write a digest as 40 lowercase hex characters plus a NUL, into `out` (>= 41 bytes):
 *         the spelling `sha1sum` and `git` print. */
void proven_sha1_to_hex(const proven_byte_t digest[PROVEN_SHA1_SIZE], char out[41]);

// -----------------------------------------------------------------------------
// MD5 (RFC 1321)
// -----------------------------------------------------------------------------

/** @brief Bytes in an MD5 digest. */
#define PROVEN_MD5_SIZE ((proven_size_t)16)

/**
 * @brief An MD5 hashing context. Opaque; use the init/update/final calls.
 *
 * Its fields are exposed only so it can live on the stack with no allocation. Do not read or
 * write them - the layout is not part of the contract.
 */
typedef struct {
    proven_u32  state[4];
    proven_u64  length;      /* total bytes fed, for the length padding */
    proven_byte_t block[64]; /* the partial 64-byte block not yet compressed */
    proven_size_t block_len;
} proven_md5_t;

/** @brief Begin an MD5 digest. */
void proven_md5_init(proven_md5_t *ctx);

/**
 * @brief Feed more bytes into a digest in progress. The result depends only on the
 *        concatenation of everything fed, not on how it was split.
 */
void proven_md5_update(proven_md5_t *ctx, proven_mem_view_t data);

/**
 * @brief Finish the digest, writing 16 bytes to `out`. The context is spent afterwards;
 *        re-init it to hash something else.
 */
void proven_md5_final(proven_md5_t *ctx, proven_byte_t out[PROVEN_MD5_SIZE]);

/**
 * @brief One-shot MD5 of a single buffer. Equivalent to init + one update + final.
 *
 * @warning Not collision resistant, and has not been for two decades. Use it only where a
 *          format fixes MD5; fingerprint content with `proven_sha256`.
 */
void proven_md5(proven_mem_view_t data, proven_byte_t out[PROVEN_MD5_SIZE]);

/** @brief Write a digest as 32 lowercase hex characters plus a NUL, into `out` (>= 33 bytes):
 *         the spelling `md5sum` prints. */
void proven_md5_to_hex(const proven_byte_t digest[PROVEN_MD5_SIZE], char out[33]);

/**
 * @brief SHA-1's and MD5's compression functions over one 64-byte block, with no padding and
 *        no length, as proven_sha256_compress (hash.h): for a constant-time MAC of the legacy
 *        TLS records. Since v0.111.0.
 */
void proven_sha1_compress(proven_u32 state[5], const proven_byte_t block[64]);
void proven_md5_compress(proven_u32 state[4], const proven_byte_t block[64]);

#endif /* PROVEN_HASH_LEGACY_H */
