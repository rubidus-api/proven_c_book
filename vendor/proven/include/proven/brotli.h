#ifndef PROVEN_BROTLI_H
#define PROVEN_BROTLI_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/allocator.h"

/**
 * @file brotli.h
 * @brief Brotli decompression (RFC 7932).
 *
 * Brotli is the format of HTTP's `Content-Encoding: br`, used mostly for files a server
 * compresses once in advance at the format's highest quality. Only decompression is here.
 *
 * The decoder works as deflate.h's does - a state object, and a step that takes what input
 * there is and fills what output space there is - and can stop anywhere: between two bits of
 * a code if that is where the input ends, in the middle of a copy if that is where the output
 * ends. Two differences:
 *
 *   - **A step may allocate.** The history that later bytes refer to - the stream's window -
 *     is kept by the decoder and grows as the stream's output grows, never past the window
 *     the stream states (2^WBITS bytes, 16 MiB at most, and 32 more). Each meta-block's
 *     prefix codes are built into a table that grows with the number of codes it has: tens of
 *     KiB for what encoders write, and 1,343,024 bytes at most - 256 codes in each of the
 *     three kinds, each as large as a code can make its table. Both are kept for the next
 *     stream. A step can therefore return PROVEN_ERR_NOMEM.
 *   - **Output can wait** in that history while the output space given is too small.
 *
 * **The window limit is the caller's.** The first bits of a stream state its window,
 * 2^WBITS - 16 bytes for WBITS from 10 to 24; a stream that needs more than the decoder's
 * limit is refused before anything is allocated for it. The default takes every stream of
 * RFC 7932. The "large window" variant, which is not RFC 7932, is refused as malformed.
 *
 * The decoder carries the format's static dictionary (122,784 bytes of words and 121 ways to
 * change them). A build with `PROVEN_NO_BROTLI` leaves the decoder and the dictionary out, and
 * proven.h does not include this header.
 *
 * Malformed input is an error, never a read or a write outside the buffers. A Brotli stream
 * has no checksum: damage that still forms a valid stream decodes to other bytes.
 *
 * Pure computation: allocation through the allocator given, no OS; part of the freestanding
 * profile.
 */

/** @brief The default window limit: 16 MiB, the largest window of RFC 7932 (2^24 - 16 bytes). */
#define PROVEN_BROTLI_WINDOW_DEFAULT ((proven_u64)16 * 1024 * 1024)

/** @brief A decompressor. Opaque; about 24 KiB when made, growing as described above. */
typedef struct proven_brotli_decoder proven_brotli_decoder_t;

/**
 * @brief Make a decompressor.
 * @param window_max The largest window a stream may need: 0 for PROVEN_BROTLI_WINDOW_DEFAULT;
 *                   otherwise at least 1 KiB (the smallest window, 1,008 bytes, is below it)
 *                   and at most 2 GiB.
 * @return PROVEN_ERR_INVALID_ARG for a bad allocator, limit or out pointer; PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_brotli_decoder_create(proven_allocator_t alloc, proven_u64 window_max, proven_brotli_decoder_t **out);

/** @brief Free a decompressor and its buffers. NULL is allowed. */
void proven_brotli_decoder_destroy(proven_brotli_decoder_t *b);

/** @brief Forget the stream in progress and expect the beginning of one. The buffers are kept. */
void proven_brotli_decoder_reset(proven_brotli_decoder_t *b);

/**
 * @brief Decompress: consume some of `in`, produce some of `out`.
 *
 * Call it again with the rest of the input, or with more output space, until `*done`.
 * `*done` is true when the stream has ended and all of its data has been produced. Input
 * after the end is not consumed - `*consumed` says where the stream ended - and later calls
 * consume nothing more; reset to read another stream.
 *
 * @param b         The decompressor.
 * @param in        Compressed bytes; may be empty.
 * @param consumed  Out: how many of them were taken.
 * @param out       Space for decompressed bytes; may be empty. Only the first `*produced`
 *                  bytes are written to.
 * @param produced  Out: how many were written.
 * @param done      Out: true at the end of the stream, as above.
 * @return PROVEN_OK, also when no progress was possible; PROVEN_ERR_INVALID_FORMAT for data
 *         that is not Brotli or is damaged; PROVEN_ERR_OUT_OF_BOUNDS for a stream whose window
 *         is above the limit; PROVEN_ERR_NOMEM; PROVEN_ERR_INVALID_ARG. After an error every
 *         call returns that error until reset.
 */
[[nodiscard]]
proven_err_t proven_brotli_decode(proven_brotli_decoder_t *b, proven_mem_view_t in, proven_size_t *consumed,
                                  proven_mem_mut_t out, proven_size_t *produced, bool *done);

/**
 * @brief Decompress a whole buffer that holds one stream into a buffer of the caller's.
 *
 * The size of `out` is the limit: a stream that would produce more is refused, however small
 * it is itself. The window limit is the default.
 *
 * @param written  Out: how many bytes were produced.
 * @return PROVEN_ERR_OUT_OF_BOUNDS when the data does not fit in `out`;
 *         PROVEN_ERR_INVALID_FORMAT also for an empty `in`, when `in` ends inside the stream
 *         and when bytes follow its end; otherwise as proven_brotli_decode.
 */
[[nodiscard]]
proven_err_t proven_brotli_decode_all(proven_allocator_t alloc, proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written);

#endif
