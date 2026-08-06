#ifndef PROVEN_STREAM_H
#define PROVEN_STREAM_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/u8str.h"
#include "proven/allocator.h"
#include "proven/fs.h"
#include "proven/fmt.h"

/**
 * @file stream.h
 * @brief Byte sinks and byte sources, behind one interface each.
 *
 * Before this existed, the library had four unrelated ways to move bytes: the
 * formatter could only write into a proven_u8str_t, a file was a proven_file_t,
 * the in-memory scanner read a proven_u8str_view_t, and the file scanner read a
 * proven_sysio_scanner_t. Four types, four function families, no common interface.
 * So you could not write one `serialize(writer, value)` that worked over both
 * memory and a file, and you could not format into a file at all - printing to one
 * meant building a whole heap string first and dumping it.
 *
 * A writer is a sink. A reader is a source. Both are small vtables passed by value,
 * exactly like proven_allocator_t, and for the same reason: the caller decides where
 * the bytes go and where they come from, and nothing is hidden.
 *
 * @note **Buffering uses memory YOU supply.** proven_writer_buffered takes a
 *       proven_mem_mut_t, the way proven_arena_create does. This library has no
 *       hidden global state and no allocation it did not ask you for - a logging
 *       path that mallocs is a logging path that fails when you most need it.
 */

// -------------------------------------------------------------
// Writer
// -------------------------------------------------------------

/**
 * @brief A byte sink.
 *
 * `write_fn` writes what it can and **reports how much went out**, even when it then
 * fails. That count is not a nicety - it is what makes a failed write recoverable.
 *
 * The first version of this interface said "write_fn must consume the whole chunk or
 * fail", which sounds tidier and is a lie a file cannot tell: a write to a pipe or a
 * full disk really does put some bytes out and then fail. A buffered writer built on
 * the tidy contract kept its whole buffer on failure and re-sent it on the next
 * flush, so a 6000-byte payload arrived as 10,096 bytes with the first 4096
 * DUPLICATED. Losing data is bad; silently doubling it is worse, because the receiver
 * cannot tell.
 *
 * `flush_fn` may be NULL, which means the writer holds nothing back.
 */
typedef struct {
    void *ctx;
    proven_result_size_t (*write_fn)(void *ctx, proven_mem_view_t chunk);
    proven_err_t (*flush_fn)(void *ctx);
} proven_writer_t;

[[nodiscard]]
static inline bool proven_writer_is_valid(proven_writer_t w) {
    return w.write_fn != (void *)0;
}

/**
 * @brief Write a whole chunk, retrying across short writes.
 * @note On failure, some of the chunk may already have gone out. There is no way for
 *       any sink to promise otherwise. Use proven_writer_write_partial if you need to
 *       know how much.
 */
[[nodiscard]]
proven_err_t proven_writer_write(proven_writer_t w, proven_mem_view_t chunk);

/** @brief Write what the sink will take, and report how much that was. */
[[nodiscard]]
proven_result_size_t proven_writer_write_partial(proven_writer_t w, proven_mem_view_t chunk);

/** @brief Write a string view. */
[[nodiscard]]
proven_err_t proven_writer_write_str(proven_writer_t w, proven_u8str_view_t view);

/**
 * @brief Push whatever the writer is holding to the thing behind it.
 *
 * @note This is not durability. A buffered writer over a file flushes its buffer
 *       into the file; getting the file onto the disk is proven_fs_sync.
 */
[[nodiscard]]
proven_err_t proven_writer_flush(proven_writer_t w);

/**
 * @brief A writer over an open file. Unbuffered: every write is a write syscall.
 */
[[nodiscard]]
proven_writer_t proven_writer_from_file(proven_file_t *file);

typedef struct {
    proven_u8str_t    *str;
    proven_allocator_t alloc;

    /**
     * @brief Once this writer has failed, it stays failed. See proven_writer_buffered_t::err.
     *
     * An allocation failure mid-render leaves the string holding a PREFIX of the output -
     * valid, terminated, and missing its end. `flush` used to answer PROVEN_OK on it, so a
     * caller who renders and then checks the flush was told a truncated document was whole.
     */
    proven_err_t err;
} proven_writer_u8str_t;

/**
 * @brief A writer that appends to an owned string, growing it.
 *
 * @note `state`, `str` and the allocator must all outlive the writer. Every writer
 *       that needs context takes a caller-owned state object - the library does not
 *       allocate one behind your back.
 */
[[nodiscard]]
proven_writer_t proven_writer_from_u8str(proven_writer_u8str_t *state, proven_u8str_t *str, proven_allocator_t alloc);

typedef struct {
    proven_mem_mut_t buf;
    proven_size_t    len;
    bool             overflowed;
} proven_writer_buf_t;

/**
 * @brief A writer over a fixed caller-owned buffer. Allocates nothing, ever.
 *
 * @note Overflow is refused, not truncated: once the buffer is full, further writes
 *       return PROVEN_ERR_OUT_OF_BOUNDS and `state->overflowed` is set. A sink that
 *       silently drops the end of your data is worse than one that says it cannot.
 */
[[nodiscard]]
proven_writer_t proven_writer_from_buffer(proven_writer_buf_t *state);

typedef struct {
    proven_writer_t  inner;
    proven_mem_mut_t buf;
    proven_size_t    len;

    /**
     * @brief Once this writer has failed, it stays failed.
     *
     * A buffered writer that has lost bytes cannot honestly continue: the byte stream it
     * was producing now has a hole in it, and the receiver has no way to see that. So the
     * failure is remembered, and every later write and flush returns it.
     *
     * Without this, `flush` answered PROVEN_OK after a write had already failed - there
     * was nothing left in the buffer, so there was nothing to fail on - and the very
     * common shape "write, write, write, check the flush" reported success on a stream
     * that was missing 10,000 bytes. A full disk is not an exotic condition.
     *
     * There is no clear() and no reset. If you have a recovery story, it involves a new
     * writer over a new sink, not pretending this one is fine.
     */
    proven_err_t err;
} proven_writer_buffered_t;

/* Reader state carries the error that ended it, because "the input stopped" and "the
 * input broke" are not the same fact. */

/**
 * @brief Wraps a writer so that small writes accumulate before reaching it.
 *
 * This is what turns ten thousand one-line writes into a handful of syscalls.
 *
 * @note The backing memory is yours, and so is the lifetime: **you must flush before
 *       the buffer or the inner writer goes away**, or you lose whatever was still
 *       held. There is no destructor to do it for you, because there is no hidden
 *       state for one to run in.
 */
[[nodiscard]]
proven_writer_t proven_writer_buffered(proven_writer_buffered_t *state, proven_writer_t inner, proven_mem_mut_t buf);

// -------------------------------------------------------------
// Reader
// -------------------------------------------------------------

/**
 * @brief A byte source.
 *
 * `read_fn` fills as much of `dest` as it can. End of input is
 * `PROVEN_ERR_EOF` - never a zero-byte success, because "I read nothing and
 * everything is fine" is the shape of a loop that never terminates.
 *
 * @note An I/O failure is **not** end of input, and must never be reported as one. A
 *       disk error that truncated a file would then be indistinguishable from a
 *       complete file, and the caller would believe it had read everything.
 */
typedef struct {
    void *ctx;
    proven_result_size_t (*read_fn)(void *ctx, proven_mem_mut_t dest);
} proven_reader_t;

[[nodiscard]]
static inline bool proven_reader_is_valid(proven_reader_t r) {
    return r.read_fn != (void *)0;
}

/** @brief Read up to `dest.size` bytes. PROVEN_ERR_EOF at end of input. */
[[nodiscard]]
proven_result_size_t proven_reader_read(proven_reader_t r, proven_mem_mut_t dest);

/** @brief A reader over an open file. */
[[nodiscard]]
proven_reader_t proven_reader_from_file(proven_file_t *file);

typedef struct {
    proven_u8str_view_t view;
    proven_size_t       cursor;
} proven_reader_view_t;

/** @brief A reader over bytes you already have. */
[[nodiscard]]
proven_reader_t proven_reader_from_view(proven_reader_view_t *state, proven_u8str_view_t view);

typedef struct {
    proven_reader_t  inner;
    proven_mem_mut_t buf;
    proven_size_t    len;      /* bytes currently held */
    proven_size_t    cursor;   /* how far into them we have read */
    bool             eof;
    proven_err_t     err;      /* the failure that stopped the source, if any */

    /* One byte of lookahead. A full buffer with no newline in it does not mean the line is too
     * long - the next byte may be the newline that ends it, or the source may have ended, in
     * which case what is held IS the final line. Both of those FIT, and neither can be told
     * from a genuinely over-long line without looking at one more byte. */
    proven_byte_t    peek;
    bool             has_peek;
} proven_reader_buffered_t;

/**
 * @brief Wraps a reader so that reads come from a buffer you supply.
 *
 * @note Required for proven_reader_read_line: a line reader has to be able to look
 *       ahead for the newline, which means holding bytes it has not handed out yet.
 */
[[nodiscard]]
proven_reader_t proven_reader_buffered(proven_reader_buffered_t *state, proven_reader_t inner, proven_mem_mut_t buf);

/**
 * @brief Reads one line, without the newline.
 *
 * There was no way to do this. The only route was proven_fs_read_all_u8str - the
 * whole file into memory - and then splitting on '\n' by hand, which is unusable for
 * a file bigger than memory and absurd for a log tail.
 *
 * @param state  a buffered reader's state; the returned view points INTO its buffer.
 * @return the line, or PROVEN_ERR_EOF when there is nothing left.
 *
 * @note The view is only valid until the next call. Copy it if it must outlive that.
 * @note A final line with no trailing newline is still returned.
 * @note A line longer than the buffer is PROVEN_ERR_OUT_OF_BOUNDS, not a silently
 *       truncated line. The buffer is yours; size it for the input you expect. A line that
 *       exactly FILLS the buffer is a line, not an error - the newline that ends it does not
 *       have to fit too, and neither does a final line that has no newline at all. (It used to
 *       be refused, which made a 4-byte file read through a 4-byte buffer unreachable.)
 * @note After an OUT_OF_BOUNDS the reader stays wedged on that line: there is no resync, and
 *       every later call returns the same error. A line you cannot hold is not a line you can
 *       skip past without deciding what to do with the bytes.
 * @note "\r\n" is handled: the '\r' is not part of the line.
 */
[[nodiscard]]
proven_result_u8str_view_t proven_reader_read_line(proven_reader_buffered_t *state);

// -------------------------------------------------------------
// Formatting straight into a writer
// -------------------------------------------------------------

/**
 * @brief Formats into a caller-supplied scratch buffer and writes it to `w`.
 *
 * This is the call that did not exist. The formatter's only sink was a
 * proven_u8str_t, so printing a formatted line to a file meant building a whole
 * heap string and dumping it - one malloc and one free per line, on the logging
 * path, which is the one place an allocation is least welcome: a program logging
 * its way out of an out-of-memory condition will fail to log it.
 *
 * @param scratch  memory YOU own. Nothing is allocated here.
 * @return PROVEN_ERR_OUT_OF_BOUNDS if the formatted text does not fit `scratch`;
 *         `required` says how much it would have needed. Nothing is written in that
 *         case - the write is atomic, so a reader never sees half a line.
 */
[[nodiscard]]
proven_fmt_result_t proven_fmt_to_writer_impl(proven_writer_t w, proven_mem_mut_t scratch,
                                              const char *fmt, const proven_arg_t *args, proven_size_t args_count);

/**
 * @brief Format one line into a writer, using a 512-byte stack buffer. No allocation.
 * @note A line longer than 511 bytes returns PROVEN_ERR_OUT_OF_BOUNDS. Use
 *       proven_fwrite_fmt with a bigger buffer when you expect long lines.
 */
#define proven_fprint(w, fmt, ...) \
    proven_fmt_to_writer_impl((w), (proven_mem_mut_t){ .ptr = (proven_byte_t[512]){0}, .size = 512 }, fmt, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

/** @brief proven_fprint with a trailing newline. */
#define proven_fprintln(w, fmt, ...) \
    proven_fprint((w), fmt "\n" __VA_OPT__(,) __VA_ARGS__)

/** @brief proven_fprint with a scratch buffer you choose the size of. */
#define proven_fwrite_fmt(w, scratch, fmt, ...) \
    proven_fmt_to_writer_impl((w), (scratch), fmt, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

#endif /* PROVEN_STREAM_H */
