#ifndef PROVEN_ZSTD_H
#define PROVEN_ZSTD_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/allocator.h"

/**
 * @file zstd.h
 * @brief Zstandard decompression and compression (RFC 8878).
 *
 * Zstandard is a newer compression format than DEFLATE: HTTP's `Content-Encoding: zstd`
 * (RFC 9659), `.zst` files, and some package and archive formats. It decodes faster than
 * DEFLATE and its compressors trade speed for size over a wider range.
 *
 * Decompression first: the decoder works as deflate.h's does - a state object, and a
 * step that takes what input there is and fills what output space there is - with two
 * differences that the format makes necessary:
 *
 *   - **A step may allocate.** Zstandard reads parts of each block from its end, so a block
 *     (at most 128 KiB compressed) is held whole before it is decoded, and its output is
 *     decoded whole too, next to the history that later blocks refer to - the frame's
 *     window. These buffers grow as a frame's output grows, never past what the frame's
 *     header says it needs, and are kept for the next frame. At most, a decoder holds its
 *     window limit plus 384 KiB plus about 16 KiB of tables. A step can therefore return
 *     PROVEN_ERR_NOMEM.
 *   - **Output can be held back** by up to one block: a block's bytes appear once it is
 *     decoded, and wait in the decoder while the output space given is too small.
 *
 * **The window limit is the caller's.** Each frame states how far back it refers, up to
 * terabytes; a frame that needs more than the decoder's limit is refused before anything is
 * allocated for it. RFC 9659 has HTTP's `zstd` coding limited to 8 MiB (8,388,608 bytes),
 * which is the default here.
 *
 * **Dictionaries** (since v0.104.0) compress short messages that resemble each other - records,
 * small API responses - far better than each alone: both sides start each frame from the same
 * "past", given out of band (RFC 8878, section 5). A raw dictionary is any bytes, at least 8; a
 * formatted one (as `zstd --train` makes) begins with the magic number 0xEC30A437, an ID, entropy
 * tables and three repeat offsets that a frame's first block may use, then its content. This
 * library uses dictionaries and does not make them. A decoder without one refuses a frame that names
 * one (`PROVEN_ERR_UNSUPPORTED`). Content sent over HTTP with the `zstd` coding does not use one (RFC
 * 8878, section 6), and nothing in the HTTP drivers does.
 *
 * A Zstandard stream is a sequence of frames. Skippable frames (magic numbers 0x184D2A50 to
 * 0x184D2A5F) are read past without producing anything. Malformed input is an error, never a
 * read or a write outside the buffers, and the output is bounded by the caller as with
 * deflate.h.
 *
 * Pure computation: allocation through the allocator given, no OS; part of the freestanding
 * profile.
 */

/** @brief The default window limit: 8 MiB, which RFC 9659 requires an HTTP decoder to accept. */
#define PROVEN_ZSTD_WINDOW_DEFAULT ((proven_u64)8 * 1024 * 1024)

/** @brief A decompressor. Opaque; about 16 KiB when made, growing as described above. */
typedef struct proven_zstd_decoder proven_zstd_decoder_t;

/**
 * @brief Make a decompressor.
 * @param window_max The largest window a frame may need: 0 for PROVEN_ZSTD_WINDOW_DEFAULT;
 *                   otherwise at least 1 KiB (the smallest window a frame can state) and at
 *                   most 2 GiB.
 * @return PROVEN_ERR_INVALID_ARG for a bad allocator, limit or out pointer; PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_zstd_decoder_create(proven_allocator_t alloc, proven_u64 window_max, proven_zstd_decoder_t **out);

/** @brief Free a decompressor and its buffers. NULL is allowed. */
void proven_zstd_decoder_destroy(proven_zstd_decoder_t *z);

/** @brief Forget the stream in progress and expect the beginning of a frame. The buffers are
 *         kept. */
void proven_zstd_decoder_reset(proven_zstd_decoder_t *z);

/**
 * @brief Decompress with a dictionary from now on, or with none again.
 *
 * Resets the decoder (call it before a stream's first frame). Every frame after that starts from
 * the dictionary: one that names this dictionary's ID or none at all (as libzstd does), while one
 * that names another is `PROVEN_ERR_UNSUPPORTED`. The bytes stay the caller's and must outlive the
 * decoder's use of them; they are read now, and a formatted dictionary's tables built (about 20 KiB,
 * allocated once). Each frame then holds the content ahead of its output - up to the content's size
 * more than without - copied once, and again only after a frame outgrew its window. While a frame has
 * produced no more than its window it may refer to any byte of the content, even further back than
 * the window (RFC 8878, section 5).
 *
 * @param dict A dictionary, raw or formatted; empty for none.
 * @return PROVEN_ERR_INVALID_ARG for fewer than 8 bytes (or a null pointer with a size);
 *         PROVEN_ERR_INVALID_FORMAT for a formatted dictionary whose tables do not read or whose
 *         repeat offsets are 0 or past its content - the decoder then has none; PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_zstd_decoder_use_dictionary(proven_zstd_decoder_t *z, proven_mem_view_t dict);

/** @brief A dictionary's ID: what a formatted one states (not 0), 0 for a raw one. */
[[nodiscard]]
proven_u32 proven_zstd_dictionary_id(proven_mem_view_t dict);

/**
 * @brief Decompress: consume some of `in`, produce some of `out`.
 *
 * Call it again with the rest of the input, or with more output space, until `*done`.
 * `*done` is true when the input given so far has ended where a frame ends - a frame that
 * produces data, or a skippable one - and all of its data has been produced. The step stops
 * there: input after a frame's end is not consumed by that call, and `*consumed` says where
 * the frame ended. To go on with the frames that follow, call it again with that input; the
 * next frame begins without a reset, and `*done` is false until it ends.
 *
 * @param z         The decompressor.
 * @param in        Compressed bytes; may be empty.
 * @param consumed  Out: how many of them were taken.
 * @param out       Space for decompressed bytes; may be empty. Only the first `*produced`
 *                  bytes are written to.
 * @param produced  Out: how many were written.
 * @param done      Out: true at the end of a frame, as above.
 * @return PROVEN_OK, also when no progress was possible; PROVEN_ERR_INVALID_FORMAT for data
 *         that is not Zstandard, is damaged, or whose checksum or stated size does not match;
 *         PROVEN_ERR_OUT_OF_BOUNDS for a frame whose window is above the limit;
 *         PROVEN_ERR_UNSUPPORTED for a frame that names a dictionary the decoder does not have; PROVEN_ERR_NOMEM;
 *         PROVEN_ERR_INVALID_ARG. After an error every call returns that error until reset.
 */
[[nodiscard]]
proven_err_t proven_zstd_decode(proven_zstd_decoder_t *z, proven_mem_view_t in, proven_size_t *consumed,
                                proven_mem_mut_t out, proven_size_t *produced, bool *done);

/**
 * @brief Decompress every frame of a whole buffer into a buffer of the caller's.
 *
 * The size of `out` is the limit: a stream that would produce more is refused, however
 * small it is itself. The window limit is the default. A frame that states its size, when
 * that fits in what is left of `out`, is decoded straight into `out` and holds no window of
 * its own; bytes of `out` past `*written` may then be written.
 *
 * @param written  Out: how many bytes were produced.
 * @return PROVEN_ERR_OUT_OF_BOUNDS when the data does not fit in `out` (or a window is above
 *         the limit); PROVEN_ERR_INVALID_FORMAT also for an empty `in` and when `in` ends
 *         inside a frame; otherwise as proven_zstd_decode.
 */
[[nodiscard]]
proven_err_t proven_zstd_decode_all(proven_allocator_t alloc, proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written);

// -----------------------------------------------------------------------------
// Compression
// -----------------------------------------------------------------------------

/*
 * The encoder works as deflate.h's compressor does: an options struct, a state object, and a
 * step with a flush. Its output is a Zstandard frame that any decoder reads - libzstd's, this
 * library's - and not the bytes libzstd would have written for the same input.
 *
 * Nineteen levels, as libzstd's first nineteen. Levels 1 to 3 are single-pass: level 1 looks up one
 * earlier position per position, by a hash of the next six bytes in a table of 2^14 entries;
 * levels 2 and 3 look up two, by hashes of eight bytes and of six (2^14 and 2^13 entries) or
 * five (2^17 and 2^16). Levels 4 to 9 keep every position in a hash chain (2^17 entries and a
 * chain of 2^16 or 2^17) and look at the first 4, 8, 12, 12, 24 and 32 of it: levels 4 and 5
 * take the best match found ("greedy"), 6 also looks one byte on for a better one ("lazy"),
 * 7 to 9 two bytes ("lazy2"). Level 6 is about gzip level 6's size at three times its speed.
 * proven_zstd_encode_all of more than 256 KiB gives levels 6 to 9 libzstd's larger tables for such
 * inputs (a hash of 2^19 to 2^21 entries and a chain of 2^18 to 2^20, each cut to the window): about
 * 1 % smaller on megabyte inputs, 2 % with a window of 8 MiB.
 * Levels 10 to 12 are lazy2 with larger tables (a hash of 2^22, 2^22 and 2^23 entries and a chain
 * of 2^21, 2^21 and 2^22, each cut to the window) and 48, 96 and 128 candidates: for files
 * compressed once and sent many times - a few megabytes a second, about 2 % smaller than level 9.
 * Levels 13 to 15 parse the same way over binary trees - every position kept in a tree of the
 * positions before it, ordered by the bytes that follow, which finds long matches without a long
 * walk (a hash of 2^22 to 2^23 entries, trees of 2^21 to 2^22 positions, 32, 64 and 128 nodes
 * compared): a few megabytes a second, 2 to 6 % smaller than level 9 on inputs above 256 KiB.
 * Levels 16 to 19 parse optimally over the same trees: every match found at a position, and the
 * three repeat offsets, weighed by what it would cost to write - from what the frame has written
 * so far - and the cheapest path through each stretch of up to 4,096 bytes taken; 18 and 19 also
 * take three-byte matches. A whole buffer of 256 KiB or less is parsed so from level 13, as libzstd
 * does. A few megabytes a second; 7 to 13 % smaller than level 9 on four kinds of input, more on
 * some (a quarter on numbered lines).
 * All try the repeat offsets first and skip ahead faster the longer they find nothing. Each block's
 * literals are Huffman-coded and its sequences FSE-coded, each table chosen per block by what
 * it would cost: the predefined one, one written for the block, the previous block's, or a
 * single symbol. A block that does not shrink is stored as it is.
 *
 * **Memory**, at most: the hash tables (64 KiB at level 1, 96 KiB at 2, 768 KiB at 3 and 4,
 * 1 MiB at 5 to 9 - and for proven_zstd_encode_all of more than 256 KiB 3 MiB at 6, 6 MiB at 7 and 8,
 * and at 9 8 MiB with the default window and 12 MiB with the largest; at 10 to 19 8 MiB with the default window, and with the largest 24 MiB at 10
 * and 11, 32 MiB at 13 and 16, 48 MiB at 12, 14, 17 and 18, 64 MiB at 15, 80 MiB at 19 - and
 * zeroed when an encoder is made or reset), at 16 to 19 about 380 KiB more for the parse (less for
 * a short whole buffer), with `long_distance` 8 bytes for each 128 bytes of window, about
 * 500 KiB for one block (its sequences, literals and output), and, for a stream, an input
 * buffer of the window and half a window (or of the window and 128 KiB, if that is more):
 * 768 KiB at the default window. Buffers grow with the input. proven_zstd_encode_all needs no
 * input buffer, and sizes its tables and block buffers to the input: a few kilobytes for a
 * short one.
 */

/** @brief When an encoder must hand over what it has. */
typedef enum {
    PROVEN_ZSTD_FLUSH_NONE = 0,    /**< more input will come: output appears as blocks fill */
    PROVEN_ZSTD_FLUSH_BLOCK = 1,   /**< everything given so far becomes decodable; the frame goes on */
    PROVEN_ZSTD_FLUSH_FINISH = 2   /**< this was the last input: end the frame */
} proven_zstd_flush_t;

/** @brief What an encoder is made with. Zero-initialise, set `alloc`, and set what applies. */
typedef struct {
    proven_allocator_t alloc;      /**< required */
    int level;                     /**< 1 (fastest) to 19 (smallest); 0: 3, as libzstd's default. (The HTTP servers' `compress_zstd_level` stays 1 to 9: they compress as they send.) */
    int window_log;                /**< how far back a match may reach, as a power of two: 10 to 23 (8 MiB, the most RFC 9659 lets an HTTP client be asked for); 0: 19 (512 KiB) */
    bool checksum;                 /**< end each frame with the low 32 bits of its XXH64 */
    proven_mem_view_t dictionary; /**< a dictionary, raw or formatted (see proven_zstd_decoder_use_dictionary): every frame starts from its content as from earlier input, and a formatted one's repeat offsets and tables - which its first block may repeat without writing them - and states its ID (a raw one's is 0). The bytes stay the caller's, read while the encoder exists. A frame made with one needs it to be decoded. Costs the content's size more in the input buffer, a copy of the content into it for each frame, and for proven_zstd_encode_all a copy of the input after it. Empty: none */
    bool long_distance;            /**< long-distance matching, as libzstd's `--long`: matches of 64 bytes or more found anywhere in the window by a hash kept for one position in about 128, the level's own matching between them. Levels 1 to 5 only: from 6 the level's own matching finds about as much, and from 9 a long match forced into the parse costs more than it saves - there the option does nothing. For large inputs that repeat from far back (archives, logs, several versions of a file): the fast levels' tables reach a few hundred kilobytes, this the whole window. Costs 8 bytes for each 128 bytes of window (512 KiB at `window_log` 23, 32 KiB at 19) and some speed. false: none */
} proven_zstd_encoder_options_t;

/** @brief A compressor. Opaque. */
typedef struct proven_zstd_encoder proven_zstd_encoder_t;

/**
 * @brief Make a compressor. Its hash tables are allocated here; its buffers as input comes.
 * @return PROVEN_ERR_INVALID_ARG for a bad allocator, level, window or out pointer, or a dictionary
 *         of fewer than 8 bytes; PROVEN_ERR_INVALID_FORMAT for a formatted dictionary that does not
 *         read (as proven_zstd_decoder_use_dictionary); PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_zstd_encoder_create(const proven_zstd_encoder_options_t *options, proven_zstd_encoder_t **out);

/** @brief Free a compressor and its buffers. NULL is allowed. */
void proven_zstd_encoder_destroy(proven_zstd_encoder_t *z);

/** @brief Forget everything and start a new frame with the same options. The buffers are kept. */
void proven_zstd_encoder_reset(proven_zstd_encoder_t *z);

/**
 * @brief Compress: consume some of `in`, produce some of `out`.
 *
 * With `PROVEN_ZSTD_FLUSH_BLOCK` or `PROVEN_ZSTD_FLUSH_FINISH`, call it again with more output
 * space - and the same flush - until all of `in` is consumed and nothing more is produced (for
 * FINISH: until `*done`). A FLUSH_BLOCK ends what was given so far as one or more blocks; the
 * frame goes on, and a decoder can give back everything up to that point. A frame made by
 * steps does not state its size; it states the window, which a decoder must hold.
 *
 * @param done Out: true once a FINISH has been carried out and all output has been produced.
 * @return PROVEN_OK, also when no progress was possible; PROVEN_ERR_INVALID_ARG;
 *         PROVEN_ERR_INVALID_STATE for input after the frame was finished (reset first);
 *         PROVEN_ERR_NOMEM, after which the compressor must be reset.
 */
[[nodiscard]]
proven_err_t proven_zstd_encode(proven_zstd_encoder_t *z, proven_mem_view_t in, proven_size_t *consumed,
                                proven_mem_mut_t out, proven_size_t *produced, proven_zstd_flush_t flush, bool *done);

/** @brief The most bytes compressing `input_size` bytes in one FINISH, or with
 *         proven_zstd_encode_all, can produce. Data that does not compress grows slightly. */
[[nodiscard]]
proven_size_t proven_zstd_encode_bound(proven_size_t input_size);

/**
 * @brief Compress a whole buffer into one frame, in a buffer of the caller's.
 *
 * The frame states its size, and when that is no more than the window, its window is the
 * size: a decoder then holds no more than the data.
 *
 * @return PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small (proven_zstd_encode_bound is always
 *         enough); otherwise as proven_zstd_encoder_create.
 */
[[nodiscard]]
proven_err_t proven_zstd_encode_all(const proven_zstd_encoder_options_t *options, proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written);

#endif
