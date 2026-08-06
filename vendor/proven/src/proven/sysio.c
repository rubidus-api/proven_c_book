#include "proven/sysio.h"
#include "proven/heap.h"
#include "../../platform/proven_sys_io.h"
#include "../../platform/proven_sys_env.h"
#include "../../platform/proven_sys_mem.h"

proven_file_t proven_sysio_stdin(void) {
    proven_sys_io_handle_t sh = proven_sys_io_std_in();
    proven_file_t f = {0};
#if defined(_WIN32) || defined(_WIN64)
    f.internal.ptr = sh.handle;
#else
    f.internal.fd = sh.fd;
#endif
    return f;
}

proven_file_t proven_sysio_stdout(void) {
    proven_sys_io_handle_t sh = proven_sys_io_std_out();
    proven_file_t f = {0};
#if defined(_WIN32) || defined(_WIN64)
    f.internal.ptr = sh.handle;
#else
    f.internal.fd = sh.fd;
#endif
    return f;
}

proven_file_t proven_sysio_stderr(void) {
    proven_sys_io_handle_t sh = proven_sys_io_std_err();
    proven_file_t f = {0};
#if defined(_WIN32) || defined(_WIN64)
    f.internal.ptr = sh.handle;
#else
    f.internal.fd = sh.fd;
#endif
    return f;
}

// -----------------------------------------------------------------------------
// The standard streams, as writers and readers
// -----------------------------------------------------------------------------

/*
 * The whole bridge is this: a standard handle parked in caller-owned storage, so that
 * stream.h's writer and reader - which take a proven_file_t * and require it to outlive them -
 * have something stable to point at. Everything else is composition; nothing here re-implements
 * what stream.c already does, which is the point. A second buffered reader would be a second
 * place for the same bug.
 */

proven_writer_t proven_sysio_stdout_writer(proven_sysio_std_t *st) {
    if (!st) return (proven_writer_t){0};
    st->file = proven_sysio_stdout();
    return proven_writer_from_file(&st->file);
}

proven_writer_t proven_sysio_stderr_writer(proven_sysio_std_t *st) {
    if (!st) return (proven_writer_t){0};
    st->file = proven_sysio_stderr();
    return proven_writer_from_file(&st->file);
}

proven_reader_t proven_sysio_stdin_reader(proven_sysio_std_t *st) {
    if (!st) return (proven_reader_t){0};
    st->file = proven_sysio_stdin();
    return proven_reader_from_file(&st->file);
}

// -----------------------------------------------------------------------------
// Buffered output
// -----------------------------------------------------------------------------

proven_writer_t proven_sysio_file_buffered(proven_sysio_out_t *st, proven_file_t file, proven_mem_mut_t buf) {
    if (!st || !buf.ptr || buf.size == 0) return (proven_writer_t){0};

    st->std.file = file;
    proven_writer_t inner = proven_writer_from_file(&st->std.file);
    return proven_writer_buffered(&st->buffered, inner, buf);
}

proven_writer_t proven_sysio_stdout_buffered(proven_sysio_out_t *st, proven_mem_mut_t buf) {
    return proven_sysio_file_buffered(st, proven_sysio_stdout(), buf);
}

// -----------------------------------------------------------------------------
// Line input
// -----------------------------------------------------------------------------

proven_err_t proven_sysio_lines_open(proven_sysio_lines_t *st, proven_file_t file, proven_mem_mut_t buf) {
    if (!st || !buf.ptr || buf.size == 0) return PROVEN_ERR_INVALID_ARG;

    st->std.file = file;
    proven_reader_t inner = proven_reader_from_file(&st->std.file);

    /* Do not assume this succeeds just because the guards above passed. If the two ever drift
     * apart, discarding the result leaves st->buffered uninitialised and the first read_line
     * hands the caller its own stack. */
    proven_reader_t r = proven_reader_buffered(&st->buffered, inner, buf);
    if (!proven_reader_is_valid(r)) return PROVEN_ERR_INVALID_ARG;

    return PROVEN_OK;
}

proven_err_t proven_sysio_stdin_lines(proven_sysio_lines_t *st, proven_mem_mut_t buf) {
    return proven_sysio_lines_open(st, proven_sysio_stdin(), buf);
}

proven_result_u8str_view_t proven_sysio_read_line(proven_sysio_lines_t *st) {
    if (!st) return (proven_result_u8str_view_t){ .err = PROVEN_ERR_INVALID_ARG };

    /*
     * Re-bind the inner reader to THIS struct's handle before reading.
     *
     * proven_sysio_lines_t contains a pointer to itself: the buffered reader's inner reader
     * addresses `&st->std.file`. A caller who moves or copies the struct - and this function
     * takes it by pointer, which is exactly the shape that says "relocatable" - would leave the
     * inner reader pointing into the original storage, which may be gone. One assignment makes
     * the implied contract true instead of a footgun.
     */
    st->buffered.inner = proven_reader_from_file(&st->std.file);

    return proven_reader_read_line(&st->buffered);
}

// -----------------------------------------------------------------------------
// Buffered Scanner for sysio (Safe for pipes/stdin)
// -----------------------------------------------------------------------------

[[nodiscard]] proven_err_t proven_sysio_scanner_init(proven_sysio_scanner_t *scanner, proven_file_t file, proven_allocator_t alloc, proven_size_t buffer_capacity) {
    if (!scanner || !proven_alloc_is_valid(alloc) || buffer_capacity == 0) return PROVEN_ERR_INVALID_ARG;

    *scanner = (proven_sysio_scanner_t){0};

    proven_result_mem_mut_t alloc_res = alloc.alloc_fn(alloc.ctx, buffer_capacity, 1);
    if (!proven_is_ok(alloc_res.err)) return alloc_res.err;
    if (!alloc_res.value.ptr) return PROVEN_ERR_INVALID_ARG;

    scanner->file = file;
    scanner->alloc = alloc;
    scanner->capacity = buffer_capacity;
    scanner->cursor = 0;
    scanner->length = 0;
    scanner->eof = false;
    scanner->buffer = (proven_u8 *)alloc_res.value.ptr;
    
    return PROVEN_OK;
}

void proven_sysio_scanner_deinit(proven_sysio_scanner_t *scanner) {
    if (!scanner) return;
    if (scanner->buffer && scanner->alloc.free_fn) {
        scanner->alloc.free_fn(scanner->alloc.ctx, scanner->buffer);
    }
    *scanner = (proven_sysio_scanner_t){0};
}

/*
 * `shifted_out` reports how far left the buffer contents moved, because a
 * caller that may need to undo the scan cannot otherwise reconcile the indices
 * it snapshotted with the compacted buffer it is looking at afterwards.
 */
static proven_err_t scanner_fill(proven_sysio_scanner_t *scanner, proven_size_t *read_bytes_out, proven_size_t *shifted_out) {
    if (read_bytes_out) *read_bytes_out = 0;
    if (shifted_out) *shifted_out = 0;
    if (!scanner || scanner->eof) return PROVEN_ERR_EOF;

    // Move remaining data to start.
    if (shifted_out) *shifted_out = scanner->cursor;
    if (scanner->cursor > 0 && scanner->cursor < scanner->length) {
        proven_size_t remaining = scanner->length - scanner->cursor;
        proven_sys_mem_move(scanner->buffer, scanner->buffer + scanner->cursor, remaining);
        scanner->length = remaining;
    } else if (scanner->cursor >= scanner->length) {
        scanner->length = 0;
    }
    scanner->cursor = 0;

    if (scanner->length >= scanner->capacity) return PROVEN_ERR_OUT_OF_BOUNDS;

#if defined(_WIN32) || defined(_WIN64)
    proven_sys_io_handle_t handle = { .handle = scanner->file.internal.ptr };
#else
    proven_sys_io_handle_t handle = { .fd = scanner->file.internal.fd };
#endif

    proven_size_t request_size = scanner->capacity - scanner->length;
    proven_sys_result_size_t read_res = proven_sys_io_read_once(handle, (char*)(scanner->buffer + scanner->length), request_size);

    if (!proven_is_ok(read_res.err)) {
        /*
         * A read that FAILED is not an end of input.
         *
         * This used to say `if (err == EOF || value == 0)`, and proven_sys_io_read_once
         * reports a failed read() as {PROVEN_ERR_IO, 0} - so every EBADF, EIO and
         * ECONNRESET took the second disjunct, latched eof, and was handed to the caller
         * as a clean, complete end of input. A stream that broke halfway through was
         * indistinguishable from one that finished.
         */
        if (read_res.err == PROVEN_ERR_EOF) {
            scanner->length += read_res.value;   /* an EOF may still carry bytes */
            if (read_bytes_out) *read_bytes_out = read_res.value;
            scanner->eof = true;
            return (read_res.value > 0) ? PROVEN_OK : PROVEN_ERR_EOF;
        }
        return read_res.err;
    }

    scanner->length += read_res.value;
    if (read_bytes_out) *read_bytes_out = read_res.value;

    /*
     * A SHORT read is not an end of input either.
     *
     * read() on a pipe, a socket or a terminal returns whatever has arrived so far -
     * that is its contract, not an error and not an EOF. This used to treat any read
     * shorter than the request as end-of-input and LATCH it, which did two things, both
     * silent: a token straddling the read boundary was committed truncated (a writer
     * that sent "123", paused, then "456 789\n" produced the integer 123, not 123456),
     * and every subsequent scan returned PROVEN_ERR_EOF while the rest of the stream sat
     * unread in the pipe. Only a zero-byte read means the input has ended - which is
     * what read() itself has always meant by it. Regular files hid the bug completely:
     * a file read is short only at real EOF.
     */
    if (read_res.value == 0) {
        scanner->eof = true;
        return PROVEN_ERR_EOF;
    }
    return PROVEN_OK;
}

typedef union {
    proven_i32 i32;
    proven_u32 u32;
    proven_i64 i64;
    proven_u64 u64;
    short s;
    unsigned short us;
    int i;
    unsigned int ui;
    long l;
    unsigned long ul;
    long long ll;
    unsigned long long ull;
    double f64;
    proven_u8str_view_t str_view;
} proven_sysio_scan_value_t;

typedef struct {
    proven_scan_arg_type_t type;
    const proven_scan_arg_t *user;
    proven_scan_arg_t scratch;
    proven_sysio_scan_value_t value;
} proven_sysio_scan_stage_t;

static void scanner_stage_init(proven_sysio_scan_stage_t *stage, const proven_scan_arg_t *arg) {
    if (!stage || !arg) return;
    stage->type = arg->type;
    stage->user = arg;
    stage->scratch = *arg;
    switch (arg->type) {
        case PROVEN_SCAN_ARG_TYPE_NONE:
            break;
        case PROVEN_SCAN_ARG_TYPE_I32:
            stage->value.i32 = 0;
            stage->scratch.ptr.i32 = arg->ptr.i32 ? &stage->value.i32 : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_U32:
            stage->value.u32 = 0;
            stage->scratch.ptr.u32 = arg->ptr.u32 ? &stage->value.u32 : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_I64:
            stage->value.i64 = 0;
            stage->scratch.ptr.i64 = arg->ptr.i64 ? &stage->value.i64 : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_U64:
            stage->value.u64 = 0;
            stage->scratch.ptr.u64 = arg->ptr.u64 ? &stage->value.u64 : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_SHORT:
            stage->value.s = 0;
            stage->scratch.ptr.s = arg->ptr.s ? &stage->value.s : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_USHORT:
            stage->value.us = 0;
            stage->scratch.ptr.us = arg->ptr.us ? &stage->value.us : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_INT:
            stage->value.i = 0;
            stage->scratch.ptr.i = arg->ptr.i ? &stage->value.i : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_UINT:
            stage->value.ui = 0;
            stage->scratch.ptr.ui = arg->ptr.ui ? &stage->value.ui : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_LONG:
            stage->value.l = 0;
            stage->scratch.ptr.l = arg->ptr.l ? &stage->value.l : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_ULONG:
            stage->value.ul = 0;
            stage->scratch.ptr.ul = arg->ptr.ul ? &stage->value.ul : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_LLONG:
            stage->value.ll = 0;
            stage->scratch.ptr.ll = arg->ptr.ll ? &stage->value.ll : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_ULLONG:
            stage->value.ull = 0;
            stage->scratch.ptr.ull = arg->ptr.ull ? &stage->value.ull : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_F64:
            stage->value.f64 = 0.0;
            stage->scratch.ptr.f64 = arg->ptr.f64 ? &stage->value.f64 : NULL;
            break;
        case PROVEN_SCAN_ARG_TYPE_STR_VIEW:
            stage->value.str_view = (proven_u8str_view_t){0};
            stage->scratch.ptr.str_view = arg->ptr.str_view ? &stage->value.str_view : NULL;
            break;
        default:
            break;
    }
}

static void scanner_stage_commit(const proven_sysio_scan_stage_t *stage) {
    if (!stage || !stage->user) return;
    switch (stage->type) {
        case PROVEN_SCAN_ARG_TYPE_NONE:
            break;
        case PROVEN_SCAN_ARG_TYPE_I32:
            if (stage->user->ptr.i32) *stage->user->ptr.i32 = stage->value.i32;
            break;
        case PROVEN_SCAN_ARG_TYPE_U32:
            if (stage->user->ptr.u32) *stage->user->ptr.u32 = stage->value.u32;
            break;
        case PROVEN_SCAN_ARG_TYPE_I64:
            if (stage->user->ptr.i64) *stage->user->ptr.i64 = stage->value.i64;
            break;
        case PROVEN_SCAN_ARG_TYPE_U64:
            if (stage->user->ptr.u64) *stage->user->ptr.u64 = stage->value.u64;
            break;
        case PROVEN_SCAN_ARG_TYPE_SHORT:
            if (stage->user->ptr.s) *stage->user->ptr.s = stage->value.s;
            break;
        case PROVEN_SCAN_ARG_TYPE_USHORT:
            if (stage->user->ptr.us) *stage->user->ptr.us = stage->value.us;
            break;
        case PROVEN_SCAN_ARG_TYPE_INT:
            if (stage->user->ptr.i) *stage->user->ptr.i = stage->value.i;
            break;
        case PROVEN_SCAN_ARG_TYPE_UINT:
            if (stage->user->ptr.ui) *stage->user->ptr.ui = stage->value.ui;
            break;
        case PROVEN_SCAN_ARG_TYPE_LONG:
            if (stage->user->ptr.l) *stage->user->ptr.l = stage->value.l;
            break;
        case PROVEN_SCAN_ARG_TYPE_ULONG:
            if (stage->user->ptr.ul) *stage->user->ptr.ul = stage->value.ul;
            break;
        case PROVEN_SCAN_ARG_TYPE_LLONG:
            if (stage->user->ptr.ll) *stage->user->ptr.ll = stage->value.ll;
            break;
        case PROVEN_SCAN_ARG_TYPE_ULLONG:
            if (stage->user->ptr.ull) *stage->user->ptr.ull = stage->value.ull;
            break;
        case PROVEN_SCAN_ARG_TYPE_F64:
            if (stage->user->ptr.f64) *stage->user->ptr.f64 = stage->value.f64;
            break;
        case PROVEN_SCAN_ARG_TYPE_STR_VIEW:
            if (stage->user->ptr.str_view) *stage->user->ptr.str_view = stage->value.str_view;
            break;
        default:
            break;
    }
}

static proven_err_t scanner_rewind_file(proven_sysio_scanner_t *scanner, proven_size_t bytes) {
    if (!scanner || bytes == 0) return PROVEN_OK;
#if defined(_WIN32) || defined(_WIN64)
    proven_sys_io_handle_t handle = { .handle = scanner->file.internal.ptr };
#else
    proven_sys_io_handle_t handle = { .fd = scanner->file.internal.fd };
#endif
    return proven_sys_io_seek_relative(handle, -((int64_t)bytes)).err;
}

proven_err_t proven_sysio_scanner_scan_impl(proven_sysio_scanner_t *scanner, const char *fmt, const proven_scan_arg_t *args, size_t args_count) {
    if (!scanner || !scanner->buffer || !fmt) return PROVEN_ERR_INVALID_ARG;
    if (args_count > 0 && !args) return PROVEN_ERR_INVALID_ARG;
    if (args_count == 0) return PROVEN_ERR_INVALID_ARG;

    /*
     * Step over leading whitespace BEFORE taking the rollback snapshot.
     *
     * Two facts collide here, and this is where they are reconciled.
     *
     * (1) A failed scan must restore the stream - that is what lets a caller try another
     *     format against the same input. (2) Whitespace has to be droppable: a run of it
     *     longer than the buffer would otherwise WEDGE the scanner forever - it cannot
     *     parse (only whitespace), it cannot refill (the buffer is full), and it cannot
     *     drop what it holds (that would be "changing the stream"). Every later scan
     *     returns PROVEN_ERR_OUT_OF_BOUNDS, permanently, on a stream that is perfectly
     *     fine. A pipe sending eight spaces to an eight-byte scanner did exactly that.
     *
     * So: whitespace ahead of a token is consumed, and the snapshot is taken AFTER. No
     * token byte is ever lost, and no scan can address the whitespace anyway - the next
     * scan would skip it too. What a failed scan restores is every byte a scan could still
     * read, which is the promise that was actually worth making.
     *
     * Doing this INSIDE the scan - after the snapshot - is what broke catastrophically:
     * scanner_fill reports how far it compacted the buffer, the rollback subtracts that
     * from the snapshot, and a cursor that moved first made the shift exceed the snapshot.
     * `start_cursor - total_shift` wrapped to ~2^64; cursor and length wrapped by the same
     * amount so every guard still compared true, and the next scan read buffer[SIZE_MAX-15].
     * ASan called it a heap-buffer-overflow. On a pipe it silently discarded the buffer.
     */
    for (;;) {
        while (scanner->cursor < scanner->length) {
            proven_u8 c = scanner->buffer[scanner->cursor];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f' && c != '\v') break;
            scanner->cursor++;
        }
        if (scanner->cursor < scanner->length) break;   /* a real byte is waiting */
        if (scanner->eof) break;                        /* nothing more is coming */

        proven_size_t pre_read = 0;
        proven_size_t pre_shift = 0;
        proven_err_t pre_err = scanner_fill(scanner, &pre_read, &pre_shift);
        if (pre_err == PROVEN_ERR_EOF) break;           /* eof is set; the check below fires */
        if (!proven_is_ok(pre_err)) return pre_err;     /* only whitespace was consumed */
    }

    proven_size_t start_cursor = scanner->cursor;
    proven_size_t start_length = scanner->length;
    bool start_eof = scanner->eof;
    proven_size_t bytes_read_total = 0;
    proven_size_t total_shift = 0;   /* how far scanner_fill compacted the buffer */

    if (scanner->cursor >= scanner->length && scanner->eof) {
        return PROVEN_ERR_EOF;
    }

    proven_allocator_t alloc = scanner->alloc;
    if (!proven_alloc_is_valid(alloc)) {
        return PROVEN_ERR_INVALID_ARG;
    }

    /* args_count comes from the caller on this public entry point, so the size
     * math is checked like every other count * size in the library. */
    proven_size_t stage_bytes;
    proven_size_t scratch_bytes;
    if (PROVEN_CKD_MUL(&stage_bytes, (proven_size_t)args_count, sizeof(proven_sysio_scan_stage_t)) ||
        PROVEN_CKD_MUL(&scratch_bytes, (proven_size_t)args_count, sizeof(proven_scan_arg_t))) {
        return PROVEN_ERR_OVERFLOW;
    }

    proven_result_mem_mut_t stage_mem = alloc.alloc_fn(alloc.ctx, stage_bytes, alignof(proven_sysio_scan_stage_t));
    if (!proven_is_ok(stage_mem.err) || !stage_mem.value.ptr) {
        return stage_mem.err;
    }

    proven_result_mem_mut_t scratch_mem = alloc.alloc_fn(alloc.ctx, scratch_bytes, alignof(proven_scan_arg_t));
    if (!proven_is_ok(scratch_mem.err) || !scratch_mem.value.ptr) {
        alloc.free_fn(alloc.ctx, stage_mem.value.ptr);
        return scratch_mem.err;
    }

    proven_sysio_scan_stage_t *stages = (proven_sysio_scan_stage_t *)stage_mem.value.ptr;
    proven_scan_arg_t *scratch_args = (proven_scan_arg_t *)scratch_mem.value.ptr;
    for (size_t i = 0; i < args_count; i++) {
        scanner_stage_init(&stages[i], &args[i]);
        scratch_args[i] = stages[i].scratch;
    }

    proven_err_t err = PROVEN_OK;
    for (;;) {
        if (scanner->cursor >= scanner->length && !scanner->eof) {
            proven_size_t read_bytes = 0;
            proven_size_t shifted = 0;
            proven_err_t fill_err = scanner_fill(scanner, &read_bytes, &shifted);
            total_shift += shifted;
            if (!proven_is_ok(fill_err)) {
                err = fill_err;
                break;
            }
            bytes_read_total += read_bytes;
        }

        if (scanner->length == 0 && scanner->eof) {
            err = PROVEN_ERR_EOF;
            break;
        }

        /* Nothing but whitespace left and the stream has ended: that IS the end of the
         * input, not a malformed token. (The cursor is NOT moved here - see the pre-scan
         * whitespace skip above for why that matters.) */
        if (scanner->eof) {
            proven_size_t ws = scanner->cursor;
            while (ws < scanner->length) {
                proven_u8 c = scanner->buffer[ws];
                if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f' && c != '\v') break;
                ws++;
            }
            if (ws == scanner->length) {
                err = PROVEN_ERR_EOF;
                break;
            }
        }

        proven_u8str_view_t view = { .ptr = (const proven_byte_t*)scanner->buffer + scanner->cursor, .size = scanner->length - scanner->cursor };
        proven_scan_t scan = proven_scan_init(view);
        err = proven_scan_fmt_internal(&scan, fmt, scratch_args, args_count);

        if (proven_is_ok(err) && !scanner->eof && (scan.cursor == scan.view.size || scan.needs_more)) {
            /* scan.needs_more covers the token that parsed as valid but might still grow -
             * a float whose exponent was cut off at the buffer boundary returns the mantissa
             * as a complete number, so cursor != view.size, yet the value is truncated. */
            err = PROVEN_ERR_NEED_MORE;
        }

        /*
         * The parse failed AT the end of what we have, and the stream has not ended: the
         * rest of the token is still in flight. Ask for it.
         *
         * Without this, a number or a literal split across a read boundary was reported as
         * malformed input - "-" then "12" was PROVEN_ERR_INVALID_ARG, "ke" then "y=7" was
         * PROVEN_ERR_NOT_FOUND - because the scan engine, which works on a complete view,
         * has always treated "the input ran out" and "the input is wrong" as the same fact.
         * For a view they ARE the same fact. For a stream they are opposites, and only the
         * engine knows which one happened, so it says so now (proven_scan_t::needs_more).
         */
        if (!proven_is_ok(err) && !scanner->eof && scan.needs_more) {
            err = PROVEN_ERR_NEED_MORE;
        }

        if (err == PROVEN_ERR_NEED_MORE) {
            proven_size_t read_bytes = 0;
            proven_size_t shifted = 0;
            proven_err_t fill_err = scanner_fill(scanner, &read_bytes, &shifted);
            total_shift += shifted;
            if (proven_is_ok(fill_err)) {
                bytes_read_total += read_bytes;
                continue;
            }
            if (fill_err == PROVEN_ERR_EOF && scanner->length > 0) {
                continue;
            }
            err = fill_err;
            break;
        }

        if (!proven_is_ok(err)) {
            break;
        }

        for (size_t i = 0; i < args_count; i++) {
            scanner_stage_commit(&stages[i]);
        }
        scanner->cursor += scan.cursor;
        err = PROVEN_OK;
        break;
    }

    if (!proven_is_ok(err)) {
        /* Undoing the scan means undoing it against the buffer we now have, not
         * the one we started with: scanner_fill compacts, so every index we
         * snapshotted moved left by total_shift. Restoring the raw snapshot
         * dropped a byte from the front of the stream and re-read bytes that had
         * already been buffered. */
        bool rewound = true;
        if (bytes_read_total > 0) {
            rewound = proven_is_ok(scanner_rewind_file(scanner, bytes_read_total));
        }
        scanner->cursor = start_cursor - total_shift;
        if (rewound) {
            /* The bytes this call pulled in are back in the file; drop them. */
            scanner->length = start_length - total_shift;
            scanner->eof = start_eof;
        }
        /* Otherwise the input does not seek (a pipe): the bytes cannot be put
         * back, so keep them buffered rather than discarding them, and leave eof
         * describing what was actually observed. */
    }

    alloc.free_fn(alloc.ctx, scratch_args);
    alloc.free_fn(alloc.ctx, stages);
    return err;
}

proven_err_t proven_sysio_print_impl(proven_file_t file, const char *fmt, const proven_arg_t *args, size_t args_count) {
    /*
     * A stack buffer first, the heap only if the line will not fit.
     *
     * This used to allocate a proven_u8str_t from the global heap for EVERY call:
     * ten thousand log lines meant ten thousand mallocs and ten thousand frees, on
     * the logging path - which is the one place an allocation is least welcome,
     * because a program logging its way out of an out-of-memory condition will fail
     * to log it.
     *
     * A typical line fits in 512 bytes and now costs zero allocations. A line that
     * does not still works: it falls back to the heap rather than being refused,
     * because refusing to print something because it is long would be worse.
     *
     * This is still one write syscall per call. That is deliberate and documented -
     * buffering would need hidden global state, which this library does not have.
     * A caller who wants ten thousand lines in a handful of syscalls builds a
     * proven_writer_buffered over their own memory; see stream.h.
     */
    proven_byte_t stack[512];
    proven_u8str_t str = proven_u8str_borrow(stack, sizeof stack);

    proven_fmt_result_t fmt_res = proven_u8str_fmt_internal((proven_allocator_t){0}, &str, false, fmt,
                                                            (proven_allocator_t){0}, args, args_count);

    proven_allocator_t heap = {0};
    bool used_heap = false;
    if (fmt_res.err == PROVEN_ERR_OUT_OF_BOUNDS) {
        heap = proven_heap_allocator();
        str = (proven_u8str_t){0};
        fmt_res = proven_u8str_fmt_internal(heap, &str, false, fmt, (proven_allocator_t){0}, args, args_count);
        used_heap = true;
    }
    if (!proven_is_ok(fmt_res.err)) {
        if (used_heap && str.internal.ptr) heap.free_fn(heap.ctx, str.internal.ptr);
        return fmt_res.err;
    }

#if defined(_WIN32) || defined(_WIN64)
    proven_sys_io_handle_t handle = { .handle = file.internal.ptr };
#else
    proven_sys_io_handle_t handle = { .fd = file.internal.fd };
#endif

    proven_sys_result_size_t w_res = proven_sys_io_write_all(handle, str.internal.ptr, str.internal.len);
    proven_err_t err = proven_is_ok(w_res.err) ? PROVEN_OK : w_res.err;

    if (used_heap && str.internal.ptr) heap.free_fn(heap.ctx, str.internal.ptr);
    return err;
}

proven_err_t proven_sysio_scan_chunk_impl(proven_file_t file, const char *fmt, const proven_scan_arg_t *args, size_t args_count) {
    char buf[4096];

    /* A string result is a borrowed view into the scanner input. This helper's input
     * lives in `buf`, so returning such a view would make it dangle as soon as this
     * function returns. Reject it before probing or reading the handle: the buffered
     * scanner is the API for callers that need a view with a documented lifetime. */
    if (args) {
        for (size_t i = 0; i < args_count; ++i) {
            if (args[i].type == PROVEN_SCAN_ARG_TYPE_STR_VIEW) {
                return PROVEN_ERR_UNSUPPORTED;
            }
        }
    }
#if defined(_WIN32) || defined(_WIN64)
    proven_sys_io_handle_t handle = { .handle = file.internal.ptr };
#else
    proven_sys_io_handle_t handle = { .fd = file.internal.fd };
#endif

    // This helper is intentionally limited to seekable inputs.
    // Probe with a zero-offset seek so pipes/stdin/sockets are rejected before any bytes are consumed.
    proven_sys_result_size_t seek_probe = proven_sys_io_seek_relative(handle, 0);
    if (!proven_is_ok(seek_probe.err)) {
        return PROVEN_ERR_UNSUPPORTED;
    }
    
    // We read up to a fixed chunk size to evaluate locally.
    proven_sys_result_size_t read_res = proven_sys_io_read_once(handle, buf, sizeof(buf));
    if (read_res.err == PROVEN_ERR_EOF || read_res.value == 0) {
        return PROVEN_ERR_NOT_FOUND;
    }
    if (!proven_is_ok(read_res.err)) {
        return read_res.err;
    }
    bool buffer_filled = (read_res.value == sizeof(buf));

    proven_u8str_view_t view = { .ptr = (const proven_byte_t*)buf, .size = read_res.value };
    proven_scan_t scan = proven_scan_init(view);
    proven_err_t err = proven_scan_fmt_internal(&scan, fmt, args, args_count);

    if (!proven_is_ok(err)) {
        if (buffer_filled && scan.cursor == read_res.value) {
            int64_t rewind_offset = -((int64_t)read_res.value);
            proven_sys_result_size_t seek_res = proven_sys_io_seek_relative(handle, rewind_offset);
            if (!proven_is_ok(seek_res.err)) {
                return seek_res.err;
            }
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
        int64_t rewind_offset = -((int64_t)read_res.value);
        proven_sys_result_size_t seek_res = proven_sys_io_seek_relative(handle, rewind_offset);
        if (!proven_is_ok(seek_res.err)) {
            return seek_res.err;
        }
        return err;
    }

    if (buffer_filled && scan.cursor == read_res.value) {
        char probe;
        proven_sys_result_size_t probe_res = proven_sys_io_read_once(handle, &probe, 1);
        if (!proven_is_ok(probe_res.err) && probe_res.err != PROVEN_ERR_EOF) {
            int64_t rewind_offset = -((int64_t)read_res.value);
            proven_sys_result_size_t seek_res = proven_sys_io_seek_relative(handle, rewind_offset);
            if (!proven_is_ok(seek_res.err)) {
                return seek_res.err;
            }
            return probe_res.err;
        }
        if (probe_res.err != PROVEN_ERR_EOF) {
            int64_t rewind_offset = -1;
            proven_sys_result_size_t seek_res = proven_sys_io_seek_relative(handle, rewind_offset);
            if (!proven_is_ok(seek_res.err)) {
                return seek_res.err;
            }
            rewind_offset = -((int64_t)read_res.value);
            seek_res = proven_sys_io_seek_relative(handle, rewind_offset);
            if (!proven_is_ok(seek_res.err)) {
                return seek_res.err;
            }
            return PROVEN_ERR_OUT_OF_BOUNDS;
        }
    }

    // Rewind any unconsumed bytes to prevent data loss (evaporation)
    if (scan.cursor < read_res.value) {
        int64_t rewind_offset = -((int64_t)(read_res.value - scan.cursor));
        proven_sys_result_size_t seek_res = proven_sys_io_seek_relative(handle, rewind_offset);
        if (!proven_is_ok(seek_res.err)) {
            err = seek_res.err;
        }
    }

    return err;
}

proven_result_u8str_t proven_env_get(proven_allocator_t alloc, proven_u8str_view_t key) {
    if (key.size > 0 && !key.ptr) {
        return (proven_result_u8str_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    if (key.size == 0) {
        return (proven_result_u8str_t){ .err = PROVEN_ERR_INVALID_ARG };
    }
    // Reject interior NUL for safety
    for (size_t i = 0; i < key.size; ++i) {
        if (key.ptr[i] == 0) return (proven_result_u8str_t){ .err = PROVEN_ERR_INVALID_ARG };
    }

    proven_size_t key_cap;
    if (PROVEN_CKD_ADD(&key_cap, key.size, 1u)) {
        return (proven_result_u8str_t){ .err = PROVEN_ERR_OVERFLOW };
    }

    char key_stack[256];
    char *key_cstr = key_stack;
    bool key_is_allocated = false;
    if (key_cap > sizeof(key_stack)) {
        if (!proven_alloc_is_valid(alloc)) {
            return (proven_result_u8str_t){ .err = PROVEN_ERR_INVALID_ARG };
        }
        proven_result_mem_mut_t key_mem = alloc.alloc_fn(alloc.ctx, key_cap, 1);
        if (!proven_is_ok(key_mem.err)) {
            return (proven_result_u8str_t){ .err = key_mem.err };
        }
        key_cstr = (char*)key_mem.value.ptr;
        key_is_allocated = true;
    }
    
    // Ensure null-terminated compatibility for PAL Layer
    for (size_t i = 0; i < key.size; ++i) {
        key_cstr[i] = (char)key.ptr[i];
    }
    key_cstr[key.size] = '\0';

    char val_buf[4096];
    size_t val_len = 0;
    proven_result_u8str_t result = {0};
    
    proven_err_t pal_err = proven_sys_env_get(key_cstr, val_buf, sizeof(val_buf), &val_len);
    
    if (pal_err == PROVEN_OK) {
        result = proven_u8str_create(alloc, val_len);
        if (!proven_is_ok(result.err)) goto cleanup_key;

        proven_err_t app_err = proven_u8str_append_grow(alloc, &result.value, (proven_u8str_view_t){.ptr = (const proven_byte_t*)val_buf, .size = val_len});
        if (!proven_is_ok(app_err)) {
            proven_u8str_destroy(alloc, &result.value);
            result = (proven_result_u8str_t){ .err = app_err };
        }
    } else if (pal_err == PROVEN_ERR_OUT_OF_BOUNDS) {
        // Large environment variable, dynamic allocation needed
        proven_size_t val_cap;
        if (PROVEN_CKD_ADD(&val_cap, val_len, 1u)) {
            result = (proven_result_u8str_t){ .err = PROVEN_ERR_OVERFLOW };
            goto cleanup_key;
        }
        if (!proven_alloc_is_valid(alloc)) {
            result = (proven_result_u8str_t){ .err = PROVEN_ERR_INVALID_ARG };
            goto cleanup_key;
        }
        proven_result_mem_mut_t alloc_res = alloc.alloc_fn(alloc.ctx, val_cap, 1);
        if (!proven_is_ok(alloc_res.err)) {
            result = (proven_result_u8str_t){ .err = alloc_res.err };
            goto cleanup_key;
        }
        
        char *big_buf = (char*)alloc_res.value.ptr;
        pal_err = proven_sys_env_get(key_cstr, big_buf, val_cap, &val_len);
        
        if (pal_err != PROVEN_OK) {
            alloc.free_fn(alloc.ctx, big_buf);
            result = (proven_result_u8str_t){ .err = pal_err };
            goto cleanup_key;
        }
        
        result = proven_u8str_create(alloc, val_len);
        if (!proven_is_ok(result.err)) {
            alloc.free_fn(alloc.ctx, big_buf);
            goto cleanup_key;
        }
        
        proven_err_t app_err = proven_u8str_append_grow(alloc, &result.value, (proven_u8str_view_t){.ptr = (const proven_byte_t*)big_buf, .size = val_len});
        alloc.free_fn(alloc.ctx, big_buf);
        
        if (!proven_is_ok(app_err)) {
            proven_u8str_destroy(alloc, &result.value);
            result = (proven_result_u8str_t){ .err = app_err };
        }
    } else {
        result = (proven_result_u8str_t){ .err = pal_err };
    }

cleanup_key:
    if (key_is_allocated) {
        alloc.free_fn(alloc.ctx, key_cstr);
    }
    return result;
}
