#ifndef PROVEN_DEFLATE_H
#define PROVEN_DEFLATE_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/allocator.h"

/**
 * @file deflate.h
 * @brief DEFLATE compression and decompression (RFC 1951), bare or framed as zlib (RFC 1950)
 *        or gzip (RFC 1952).
 *
 * DEFLATE is the compression inside gzip files, HTTP's `Content-Encoding: gzip`, PNG, ZIP
 * and WebSocket's `permessage-deflate`. It replaces repeated stretches of bytes by "the same
 * as 300 bytes ago, for 40 bytes" and then writes what is left with shorter codes for what is
 * common. Text shrinks to a third or a quarter; data that is already compressed or encrypted
 * does not shrink at all.
 *
 * Both directions work the same way: a state object, and a step that takes what input there
 * is and fills what output space there is.
 *
 *   - A step never allocates and never writes past the output it was given.
 *   - `PROVEN_OK` with nothing consumed and nothing produced means it needs more of one or
 *     the other.
 *   - `*done` becomes true when the stream has ended - when decompressing, where the stream
 *     said so; when compressing, after everything asked for by `PROVEN_DEFLATE_FINISH` is out.
 *
 * **Decompression is where the care goes.** The input may come from anyone. A kilobyte of
 * DEFLATE can expand to a megabyte, so the caller always decides how much output it will
 * take: the step function by the buffer it passes, the whole-buffer helper by a required
 * limit. Malformed input is an error, never a read or a write outside the buffers.
 *
 * Pure computation: one allocation when a state is made, no OS; part of the freestanding
 * profile.
 */

/** @brief How the compressed data is wrapped. */
typedef enum {
    PROVEN_DEFLATE_RAW = 0,   /**< DEFLATE and nothing else: no header, no checksum (`permessage-deflate`, ZIP entries) */
    PROVEN_DEFLATE_ZLIB = 1,  /**< a two-byte header and an Adler-32 of the data (PNG; HTTP's `deflate`) */
    PROVEN_DEFLATE_GZIP = 2   /**< a gzip member: header, CRC-32 and length (`.gz` files; HTTP's `gzip`) */
} proven_deflate_format_t;

/** @brief A decompressor. Opaque; about 35 KiB, allocated when made. */
typedef struct proven_inflate proven_inflate_t;

/**
 * @brief Make a decompressor for one framing.
 * @return PROVEN_ERR_INVALID_ARG for a bad allocator, format or out pointer; PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_inflate_create(proven_allocator_t alloc, proven_deflate_format_t format, proven_inflate_t **out);

/** @brief Free a decompressor. NULL is allowed. */
void proven_inflate_destroy(proven_inflate_t *z);

/** @brief Forget everything and start a new stream of the same framing - also the way to go
 *         on after `done` when gzip members follow each other. */
void proven_inflate_reset(proven_inflate_t *z);

/**
 * @brief Start a new stream of the same framing after `done`, **keeping the last 32 KiB**: the
 *        new stream's matches may reach back into what the one before produced.
 *
 * This is for `permessage-deflate` with context takeover (RFC 7692), where one decompressor
 * reads every message of a connection: a sender may end a message with a final block instead
 * of a flush (section 7.2.3.4), and its next message still refers back to the earlier ones.
 * For a zlib or gzip stream it is a reset whose kept bytes nothing refers to.
 */
void proven_inflate_restart(proven_inflate_t *z);

/**
 * @brief Decompress: consume some of `in`, produce some of `out`.
 *
 * Call it again with the rest of the input, or with more output space, until `*done`. Input
 * after the end of the stream is not consumed: `*consumed` says where the stream ended.
 *
 * Only the first `*produced` bytes of `out` are data. Up to seven bytes after them, inside
 * the space given, may have been written to as well - a copy made eight bytes at a time - so
 * give as `out` space that is free, not space whose later bytes are still wanted.
 *
 * A raw stream need not end. One that is cut at flush points instead - `permessage-deflate`
 * with context takeover - is fed piece by piece in the same way: `*done` simply never
 * becomes true, and the history the next piece refers to is kept.
 *
 * @param z         The decompressor.
 * @param in        Compressed bytes; may be empty.
 * @param consumed  Out: how many of them were taken.
 * @param out       Space for decompressed bytes; may be empty.
 * @param produced  Out: how many were written.
 * @param done      Out: true once the stream has ended and all of its data has been produced.
 * @return PROVEN_OK, also when no progress was possible; PROVEN_ERR_INVALID_FORMAT for data
 *         that is not a valid stream of this framing or whose checksum does not match;
 *         PROVEN_ERR_UNSUPPORTED for a zlib stream that needs a preset dictionary;
 *         PROVEN_ERR_INVALID_ARG. After an error every call returns that error until reset.
 */
[[nodiscard]]
proven_err_t proven_inflate(proven_inflate_t *z, proven_mem_view_t in, proven_size_t *consumed,
                            proven_mem_mut_t out, proven_size_t *produced, bool *done);

/**
 * @brief Decompress a whole buffer into a buffer of the caller's.
 *
 * The size of `out` is the limit: a stream that would produce more is refused, however
 * small it is itself. There is deliberately no form of this that grows its output.
 *
 * @param written  Out: how many bytes were produced.
 * @param consumed Out, may be NULL: how many bytes of `in` the stream took.
 * @return PROVEN_ERR_OUT_OF_BOUNDS when the data does not fit in `out`;
 *         PROVEN_ERR_INVALID_FORMAT also when `in` ends before the stream does; otherwise as
 *         proven_inflate.
 */
[[nodiscard]]
proven_err_t proven_inflate_all(proven_allocator_t alloc, proven_deflate_format_t format, proven_mem_view_t in,
                                proven_mem_mut_t out, proven_size_t *written, proven_size_t *consumed);

/** @brief When a compressor must hand over what it has. */
typedef enum {
    PROVEN_DEFLATE_FLUSH_NONE = 0,   /**< more input will come: output appears when a block is full */
    PROVEN_DEFLATE_FLUSH_SYNC = 1,   /**< everything given so far becomes decodable, ending on a byte boundary; the stream goes on */
    PROVEN_DEFLATE_FLUSH_FINISH = 2  /**< this was the last input: end the stream */
} proven_deflate_flush_t;

/** @brief What a compressor is made with. Zero-initialise, set `alloc`, and set what applies. */
typedef struct {
    proven_allocator_t alloc;        /**< required */
    proven_deflate_format_t format;  /**< the framing; zero is PROVEN_DEFLATE_RAW */
    int level;                       /**< 1 (fastest) to 9 (smallest); 0: the default, 6; -1: do not compress, only store */
    int window_bits;                 /**< how far back a match may reach, as a power of two: 9 to 15; 0: 15 (32 KiB) */
} proven_deflate_options_t;

/** @brief A compressor. Opaque; its size follows `window_bits` - about 325 KiB at the
 *         default, about 11 KiB at 9. */
typedef struct proven_deflate proven_deflate_t;

/**
 * @brief Make a compressor.
 * @return PROVEN_ERR_INVALID_ARG for a bad allocator, format, level or window; PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_deflate_create(const proven_deflate_options_t *options, proven_deflate_t **out);

/** @brief Free a compressor. NULL is allowed. */
void proven_deflate_destroy(proven_deflate_t *z);

/** @brief Forget everything and start a new stream with the same options. */
void proven_deflate_reset(proven_deflate_t *z);

/**
 * @brief Compress: consume some of `in`, produce some of `out`.
 *
 * With `PROVEN_DEFLATE_FLUSH_FINISH`, call it again with more output space - and the same
 * flush - until `*done`. Output is valid DEFLATE that any decompressor reads; it is not the
 * bytes another compressor would have produced for the same input.
 *
 * `PROVEN_DEFLATE_FLUSH_SYNC` ends what was given so far with an empty stored block, so the
 * last four bytes produced are `00 00 ff ff` - the bytes `permessage-deflate` leaves off.
 *
 * @param done Out: true once a FINISH has been carried out and all output has been produced.
 * @return PROVEN_OK, also when no progress was possible; PROVEN_ERR_INVALID_ARG;
 *         PROVEN_ERR_INVALID_STATE for input after the stream was finished.
 */
[[nodiscard]]
proven_err_t proven_deflate(proven_deflate_t *z, proven_mem_view_t in, proven_size_t *consumed,
                            proven_mem_mut_t out, proven_size_t *produced, proven_deflate_flush_t flush, bool *done);

/** @brief The most bytes compressing `input_size` bytes in one FINISH can produce, for sizing
 *         the buffer of proven_deflate_all. Data that does not compress grows slightly. */
[[nodiscard]]
proven_size_t proven_deflate_bound(proven_deflate_format_t format, proven_size_t input_size);

/**
 * @brief Compress a whole buffer into a buffer of the caller's.
 * @return PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small (proven_deflate_bound is always
 *         enough); otherwise as proven_deflate_create.
 */
[[nodiscard]]
proven_err_t proven_deflate_all(const proven_deflate_options_t *options, proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written);

#endif
