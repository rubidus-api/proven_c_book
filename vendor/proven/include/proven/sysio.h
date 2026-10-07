#ifndef PROVEN_SYSIO_H
#define PROVEN_SYSIO_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/u8str.h"
#include "proven/fmt.h"
#include "proven/fs.h"
#include "proven/scan.h"
#include "proven/stream.h"

/**
 * @file sysio.h
 * @brief The standard streams, and the console I/O that replaces <stdio.h>.
 *
 * stdin, stdout and stderr are files, and - since this header met `stream.h` - they are also
 * writers and readers. That is the whole point of the bridge below: everything `stream.h` can
 * do to a byte sink or a byte source, it can now do to a standard stream. You can
 * `proven_fprintln` to stderr, wrap stdout in a buffered writer so a thousand small prints
 * cost one syscall instead of a thousand, and read stdin a line at a time - which, until this
 * bridge existed, there was no way to do at all.
 *
 * The direct calls (`proven_print`, `proven_println`, `proven_eprint`) are unchanged and
 * remain unbuffered: what they write is on its way out before they return, so nothing is lost
 * if the program dies. Buffering is opt-in precisely because it is not free of consequence --
 * buffered output that is never flushed is output that never happened.
 */

// -----------------------------------------------------------------------------
// Standard Data Streams
// -----------------------------------------------------------------------------

/**
 * @brief Retrieves the standard input handle.
 */
[[nodiscard]] proven_file_t proven_sysio_stdin(void);

/**
 * @brief Retrieves the standard output handle.
 */
[[nodiscard]] proven_file_t proven_sysio_stdout(void);

/**
 * @brief Retrieves the standard error handle.
 */
[[nodiscard]] proven_file_t proven_sysio_stderr(void);

// -----------------------------------------------------------------------------
// The standard streams, as writers and readers
// -----------------------------------------------------------------------------

/**
 * @brief Somewhere to keep a standard handle, so a writer or reader can point at it.
 *
 * `proven_writer_from_file` takes a `proven_file_t *`, and the file has to outlive the
 * writer - so it cannot be a temporary. This struct is that storage, and it is yours: declare
 * one on the stack next to the writer that uses it.
 *
 * @warning **Do not copy or move these state structs once a writer or reader has been made
 *          from one.** The writer holds a pointer INTO the struct, so a copy leaves the
 *          original addressed and the copy inert - and if the original goes out of scope, the
 *          writer is left pointing at dead storage. This is the same rule that governs
 *          `stream.h`'s own state structs (`proven_writer_buffered_t` and friends): the state
 *          stays where you declared it, for as long as the writer or reader lives.
 *          (`proven_sysio_lines_t` is the exception - `proven_sysio_read_line` re-binds it on
 *          every call, so a line reader may be moved.)
 */
/**
 * @brief Text in flight at a Windows console edge. Not for the caller to touch.
 *
 * Writing: the first bytes of a UTF-8 character whose rest is in the next write - a buffered
 * writer flushes at its buffer size, not at a character boundary. Reading: UTF-8 bytes
 * converted but not yet handed out, a high surrogate waiting for its low half, and whether the
 * console has already produced malformed text that the next read must report.
 */
typedef struct {
    proven_byte_t bytes[4];
    proven_u8     len;
    proven_u16    high;
    bool          has_high;
    bool          broken;
} proven_sysio_carry_t;

typedef struct {
    proven_file_t file;

    /**
     * @brief Set when `file` is a Windows console; always false on POSIX.
     *
     * A console decodes the bytes it is given in its own code page, so UTF-8 written to it
     * with WriteFile is mojibake unless someone ran `chcp 65001` - and reading it with
     * ReadFile returns code-page bytes, not UTF-8. So the writers and readers made from this
     * struct talk to a console in UTF-16 (WriteConsoleW / ReadConsoleW), converting at the
     * edge, and the rest of the program never sees anything but UTF-8. Files, pipes and
     * redirected streams are not consoles and keep getting the bytes exactly as written.
     *
     * `proven_writer_from_file` / `proven_reader_from_file` on a console handle are byte-exact
     * and do NOT convert: make console writers and readers here.
     */
    bool console;
    proven_sysio_carry_t carry;
} proven_sysio_std_t;

/** @brief An unbuffered writer over stdout. Every write is a write syscall. */
[[nodiscard]] proven_writer_t proven_sysio_stdout_writer(proven_sysio_std_t *st);

/** @brief An unbuffered writer over stderr. Every write is a write syscall - which is what
 *         you want for an error: it is out before the next line of code runs. */
[[nodiscard]] proven_writer_t proven_sysio_stderr_writer(proven_sysio_std_t *st);

/** @brief A reader over stdin. */
[[nodiscard]] proven_reader_t proven_sysio_stdin_reader(proven_sysio_std_t *st);

// -----------------------------------------------------------------------------
// Buffered output: many small prints, one syscall
// -----------------------------------------------------------------------------

/**
 * @brief A buffered writer over a standard stream, and the buffer it writes through.
 *
 * Declare one, hand it a buffer you own, and `proven_fprintln` into the writer it gives back.
 * Small writes accumulate; the OS sees one write per full buffer instead of one per line.
 */
typedef struct {
    proven_sysio_std_t std;
    proven_writer_buffered_t buffered;
} proven_sysio_out_t;

/**
 * @brief Wrap stdout in a buffered writer over `buf`.
 *
 * @warning **You must flush it.** Nothing reaches the terminal until the buffer fills or you
 *          call `proven_writer_flush`. Buffered output that is never flushed is output that
 *          never happened - and unlike C's `stdout`, nothing here flushes behind your back at
 *          exit, because a library that registers an atexit handler you did not ask for is a
 *          library that owns your process. Flush before you return, and flush before you
 *          print anything to stderr that is supposed to appear after it.
 */
[[nodiscard]] proven_writer_t proven_sysio_stdout_buffered(proven_sysio_out_t *st, proven_mem_mut_t buf);

/**
 * @brief The same, over any open file.
 * @note A zero-initialised `proven_file_t` is not an invalid handle - on POSIX it is fd 0,
 *       which is stdin. Pass a file you actually opened; the library cannot tell a handle
 *       you forgot to fill in from one that legitimately refers to fd 0.
 */
[[nodiscard]] proven_writer_t proven_sysio_file_buffered(proven_sysio_out_t *st, proven_file_t file, proven_mem_mut_t buf);

// -----------------------------------------------------------------------------
// Line input: the operation that had no route
// -----------------------------------------------------------------------------

/**
 * @brief A line reader over a standard stream or a file, and the buffer it reads through.
 */
typedef struct {
    proven_sysio_std_t std;
    proven_reader_buffered_t buffered;
} proven_sysio_lines_t;

/**
 * @brief Open a line reader over `file`, reading through `buf`.
 *
 * @return PROVEN_ERR_INVALID_ARG for a null state or an empty buffer.
 * @note Size `buf` for the longest LINE you expect. A longer line is
 *       PROVEN_ERR_OUT_OF_BOUNDS, never a silently truncated one.
 */
[[nodiscard]] proven_err_t proven_sysio_lines_open(proven_sysio_lines_t *st, proven_file_t file, proven_mem_mut_t buf);

/**
 * @brief Open a line reader over stdin.
 *
 * Reading stdin a line at a time - the single most common thing a program does with it - had
 * no route through this library: the choices were the token scanner, or reading the whole of
 * a stream that may never end. This is the missing one.
 */
[[nodiscard]] proven_err_t proven_sysio_stdin_lines(proven_sysio_lines_t *st, proven_mem_mut_t buf);

/**
 * @brief The next line, without its newline. PROVEN_ERR_EOF when the input is done.
 *
 * @note The view points INTO `buf` and is only valid until the next call. Copy it if it must
 *       outlive that. This is what makes reading a million lines cost one buffer rather than
 *       a million allocations.
 * @note "\r\n" is handled; a final line with no trailing newline is still returned.
 */
[[nodiscard]] proven_result_u8str_view_t proven_sysio_read_line(proven_sysio_lines_t *st);

#ifndef PROVEN_NO_U16STR
/**
 * @brief A UTF-16 line reader over a standard stream or a file.
 *
 * The u16 twin of proven_sysio_lines_t: lines come back as `proven_u16str_view_t`, decoded
 * from `enc` (stream.h's proven_u16_reader_t does the work). For the text a person types or
 * pipes in, that is PROVEN_TEXT_UTF8; for a file whose encoding you do not control,
 * PROVEN_TEXT_AUTO lets a byte order mark decide.
 */
typedef struct {
    proven_sysio_std_t  std;
    proven_u16_reader_t reader;
} proven_sysio_u16_lines_t;

/**
 * @brief Open a UTF-16 line reader over `file`, reading `enc`, through `buf` of `cap` units.
 * @return PROVEN_ERR_INVALID_ARG for a null state, a buffer under 2 units, or a bad encoding.
 */
[[nodiscard]] proven_err_t proven_sysio_u16_lines_open(proven_sysio_u16_lines_t *st, proven_file_t file,
                                                       proven_text_encoding_t enc, proven_u16 *buf, proven_size_t cap);

/** @brief Open a UTF-16 line reader over stdin, which is read as UTF-8. */
[[nodiscard]] proven_err_t proven_sysio_stdin_u16_lines(proven_sysio_u16_lines_t *st, proven_u16 *buf, proven_size_t cap);

/**
 * @brief The next line as UTF-16, without its newline. PROVEN_ERR_EOF when the input is done.
 * @note The view points into `buf` and lasts until the next call. The struct may be moved
 *       between calls: its reader is re-bound to it each time, as proven_sysio_read_line does.
 */
[[nodiscard]] proven_result_u16str_view_t proven_sysio_read_u16_line(proven_sysio_u16_lines_t *st);
#endif /* PROVEN_NO_U16STR */

// -----------------------------------------------------------------------------
// Buffered Scanner for sysio (Safe for pipes/stdin)
// -----------------------------------------------------------------------------

/*
 * Two buffered readers, and which one you want.
 *
 * This header offers a buffered TOKEN scanner (below) and, above, a buffered LINE reader built
 * on stream.h. Having two is a fair thing to be suspicious of, so here is the distinction,
 * stated once:
 *
 *   - proven_sysio_lines_t / proven_sysio_read_line  -> you want LINES, or bytes.
 *     It is stream.h's reader, so it composes with everything else there. Reach for this by
 *     default; it is the one most programs want.
 *
 *   - proven_sysio_scanner_t / proven_sysio_scanner_scan  -> you want TYPED TOKENS.
 *     "{} {} {}" into an int, a double and a string view, out of a pipe, without ever
 *     committing a half-parsed value.
 *
 * They are not the same mechanism wearing two hats. A line reader looks for a delimiter; a
 * token scanner has to hand the raw bytes to a parser that may reject them, and then put the
 * stream back exactly as it was - it restores the cursor (and the file position, where the
 * source can seek) so a failed scan consumes nothing. A refill in the middle of a token has to
 * be undoable in a way that a refill in the middle of a line never does.
 *
 * They were not unified because unifying them would mean giving the line reader a rollback it
 * has no use for, or giving the scanner a hot path it cannot use - and because the scanner is
 * the more delicate of the two and is heavily tested where it is. Two mechanisms with two
 * jobs, said out loud, beats one mechanism pretending to have one job.
 */

/**
 * @brief Buffered scanner for system I/O, providing safe scanning for both
 * seekable and non-seekable streams (pipes, stdin).
 */
typedef struct {
    proven_file_t file;
    proven_allocator_t alloc;
    proven_u8 *buffer;
    proven_size_t capacity;
    proven_size_t cursor;
    proven_size_t length;
    bool eof;
    bool console;               /**< a Windows console: read as UTF-16, handed out as UTF-8 */
    proven_sysio_carry_t carry; /**< see proven_sysio_std_t; not for the caller to touch */
} proven_sysio_scanner_t;

/**
 * @brief Initializes a buffered sysio scanner.
 *
 * @note Size `buffer_capacity` for the longest TOKEN you expect. Whitespace in front of a
 *       token does not count: it is consumed before the scan proper, so a run of it longer
 *       than the buffer is not a problem. A token that does not fit is
 *       PROVEN_ERR_OUT_OF_BOUNDS, never a truncated value.
 * @note A failed scan restores every byte a scan could still read. It may consume the
 *       whitespace ahead of the token - no scan can address that whitespace anyway, and a
 *       scanner that could not drop it would be wedged forever by a whitespace run longer
 *       than its buffer.
 * @param scanner The scanner object to initialize.
 * @param file The file handle to read from.
 * @param alloc The allocator for the internal buffer. The allocator must satisfy
 *        the full proven_allocator_t contract.
 * @param buffer_capacity The size of the internal buffer (e.g., 4096).
 * @return PROVEN_ERR_INVALID_ARG for a null scanner, an invalid allocator, or a
 *         zero buffer size. On failure, the scanner is left zero-safe.
 */
[[nodiscard]] proven_err_t proven_sysio_scanner_init(proven_sysio_scanner_t *scanner, proven_file_t file, proven_allocator_t alloc, proven_size_t buffer_capacity);

/**
 * @brief Deinitializes the sysio scanner and frees its internal buffer.
 */
void proven_sysio_scanner_deinit(proven_sysio_scanner_t *scanner);

/**
 * @brief Low-level implementation for buffered scanning.
 *
 * The scanner refills and retries when a token reaches the end of the current
 * loaded fragment before EOF. If the source is exhausted, it returns EOF; if a
 * token cannot fit within the fixed buffer after refill, it returns
 * PROVEN_ERR_OUT_OF_BOUNDS and restores the scanner state and file position on
 * seekable inputs instead of accepting a truncated token.
 *
 * A successful string-view destination borrows the scanner's internal buffer.
 * The view remains valid only until the next scanner scan call or scanner
 * deinitialization, either of which may refill, compact, or free that buffer.
 */
/*
 * MACRO SUPPORT for proven_sysio_scanner_scan (RFC-0009 X-004).
 * Not a stable interface: the signature may change in any MINOR release. Call the macro.
 */
[[nodiscard]]
proven_err_t proven_sysio_scanner_scan_impl(proven_sysio_scanner_t *scanner, const char *fmt, const proven_scan_arg_t *args, size_t args_count);

/**
 * @brief Type-safe formatted scanning using a buffered scanner.
 */
#define proven_sysio_scanner_scan(scanner, fmt, ...) \
    proven_sysio_scanner_scan_impl(scanner, fmt, \
        ((proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_scan_arg_t)))

// -----------------------------------------------------------------------------
// Type-Safe Printing Console macros
// -----------------------------------------------------------------------------

/* Deliberately not [[nodiscard]]: proven_print / proven_eprint expand to this
 * and are used as printf is - a failed write to a console is conventionally
 * ignored. The scan entry points above and below are annotated, because
 * dropping their error means reading data that was never parsed. */
/*
 * MACRO SUPPORT for proven_print, proven_println, proven_eprint and proven_eprintln (RFC-0009 X-004).
 * Not a stable interface: the signature may change in any MINOR release. Call the macro.
 */
proven_err_t proven_sysio_print_impl(proven_file_t handle, const char *fmt, const proven_arg_t *args, size_t args_count);
/**
 * @brief Type-safe formatted scanning from a file descriptor.
 *
 * Reads at most one fixed-size chunk (4096 bytes). The helper is intended for
 * seekable inputs; if the handle cannot be rewound, it returns
 * PROVEN_ERR_UNSUPPORTED before consuming data. If the chunk fills before a
 * complete token is available, the function returns PROVEN_ERR_OUT_OF_BOUNDS
 * instead of accepting a silently truncated parse, and the file cursor is restored
 * to the start of the chunk.
 *
 * String-view destinations are not supported and return PROVEN_ERR_UNSUPPORTED
 * before any input is consumed. A scanned string view borrows the scanner input,
 * but this helper's input buffer is local to the call and cannot safely escape.
 * Use proven_sysio_scanner_t when the caller needs a borrowed string result; its
 * result remains valid only until the next scan call or scanner deinitialization.
 */
/*
 * MACRO SUPPORT for proven_scan_fmt_from_file and proven_scan_fmt_from_stdin (RFC-0009 X-004).
 * Not a stable interface: the signature may change in any MINOR release. Call the macro.
 */
[[nodiscard]]
proven_err_t proven_sysio_scan_chunk_impl(proven_file_t handle, const char *fmt, const proven_scan_arg_t *args, size_t args_count);

/**
 * @brief Type-safe formatted printing to stdout.
 * Replacing printf(fmt, ...).
 */
#define proven_print(fmt, ...) \
    proven_sysio_print_impl(proven_sysio_stdout(), fmt, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

/**
 * @brief Type-safe formatted printing to stdout with a trailing newline.
 * Replacing printf(fmt "\n", ...).
 */
#define proven_println(fmt, ...) \
    proven_print(fmt "\n" __VA_OPT__(,) __VA_ARGS__)

/**
 * @brief Type-safe formatted printing to stderr.
 * Replacing fprintf(stderr, fmt, ...).
 */
#define proven_eprint(fmt, ...) \
    proven_sysio_print_impl(proven_sysio_stderr(), fmt, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

/**
 * @brief Type-safe formatted printing to stderr with a trailing newline.
 */
#define proven_eprintln(fmt, ...) \
    proven_eprint(fmt "\n" __VA_OPT__(,) __VA_ARGS__)

// -----------------------------------------------------------------------------
// Type-Safe Scanning Console macros
// -----------------------------------------------------------------------------

/**
 * @brief Type-safe formatting scanner from a file descriptor.
 */
#define proven_scan_fmt_from_file(file, fmt, ...) \
    proven_sysio_scan_chunk_impl(file, fmt, \
        (proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }, \
        (sizeof((proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_scan_arg_t)))

/**
 * @brief Type-safe formatting scanner from standard input.
 */
#define proven_scan_fmt_from_stdin(fmt, ...) \
    proven_scan_fmt_from_file(proven_sysio_stdin(), fmt __VA_OPT__(,) __VA_ARGS__)

// -----------------------------------------------------------------------------
// Environment Variables
// -----------------------------------------------------------------------------

/**
 * @brief Fetches an environment variable by name.
 * @param alloc The allocator to securely hold the resulting string.
 * @param key The environment variable name view.
 * @return An error if not found, or a dynamically allocated string containing the value.
 *
 * @note PROVEN_ERR_NOT_FOUND when the variable is not set. A variable set to the empty string
 *       is PROVEN_OK with an empty string, on every platform.
 * @note PROVEN_ERR_INVALID_ENCODING when the value is not valid text: bytes that are not
 *       UTF-8 on POSIX, a lone surrogate on Windows. Nothing is substituted - the strict rule
 *       of the rest of the library. A program that must pass such a value through untouched
 *       has to read it with the platform's own call.
 */
[[nodiscard]] proven_result_u8str_t proven_env_get(proven_allocator_t alloc, proven_u8str_view_t key);

#endif /* PROVEN_SYSIO_H */
