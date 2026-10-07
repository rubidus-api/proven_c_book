#include "proven/stream.h"
#include "../../platform/proven_sys_mem.h"

// -------------------------------------------------------------
// Writer
// -------------------------------------------------------------

proven_result_size_t proven_writer_write_partial(proven_writer_t w, proven_mem_view_t chunk) {
    proven_result_size_t res = {0};
    if (!proven_writer_is_valid(w)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (chunk.size == 0) return res;   /* OK, zero bytes */
    if (!chunk.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    return w.write_fn(w.ctx, chunk);
}

proven_err_t proven_writer_write(proven_writer_t w, proven_mem_view_t chunk) {
    proven_size_t done = 0;
    while (done < chunk.size) {
        proven_mem_view_t rest = { .ptr = chunk.ptr + done, .size = chunk.size - done };
        proven_result_size_t r = proven_writer_write_partial(w, rest);
        if (!proven_is_ok(r.err)) return r.err;
        if (r.value == 0) return PROVEN_ERR_IO;   /* no progress: a sink that will never take it */
        done += r.value;
    }
    return PROVEN_OK;
}

proven_err_t proven_writer_write_str(proven_writer_t w, proven_u8str_view_t view) {
    return proven_writer_write(w, proven_mem_view_from_u8(view));
}

proven_err_t proven_writer_flush(proven_writer_t w) {
    if (!proven_writer_is_valid(w)) return PROVEN_ERR_INVALID_ARG;
    if (!w.flush_fn) return PROVEN_OK;   /* nothing is held back */
    return w.flush_fn(w.ctx);
}

/* --- a file ----------------------------------------------------------- */

static proven_result_size_t writer_file_write(void *ctx, proven_mem_view_t chunk) {
    proven_result_size_t res = {0};
    proven_file_t *file = (proven_file_t *)ctx;
    if (!file) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    /* proven_fs_write is a single write: it reports exactly how many bytes the OS
     * took. That count is the whole point - proven_fs_write_all would swallow it and
     * report only the error, which is how the buffered writer used to duplicate the
     * prefix that had already gone out. */
    return proven_fs_write(*file, chunk);
}

proven_writer_t proven_writer_from_file(proven_file_t *file) {
    if (!file) return (proven_writer_t){0};
    return (proven_writer_t){ .ctx = file, .write_fn = writer_file_write, .flush_fn = (void *)0 };
}

/* --- an owned string --------------------------------------------------- */

static proven_result_size_t writer_u8str_write(void *ctx, proven_mem_view_t chunk) {
    proven_result_size_t res = {0};
    proven_writer_u8str_t *s = (proven_writer_u8str_t *)ctx;
    if (!s || !s->str) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (!proven_is_ok(s->err)) {
        /* A chunk is already missing from the middle of this document. Appending more
         * would produce a string that reads as complete output and is not. */
        res.err = s->err;
        return res;
    }
    proven_u8str_view_t view = { .ptr = chunk.ptr, .size = chunk.size };
    /* append_grow is failure-atomic: it takes all of it or none of it. */
    res.err = proven_u8str_append_grow(s->alloc, s->str, view);
    if (!proven_is_ok(res.err)) {
        s->err = res.err;
        return res;
    }
    res.value = chunk.size;
    return res;
}

/*
 * The allocation that fails mid-render leaves the string holding a PREFIX of the output -
 * valid, NUL-terminated, and missing its end. There is nothing to push out (the string IS
 * the sink), so this writer had no flush at all and proven_writer_flush answered PROVEN_OK
 * for it. "Render, render, render, check the flush" is what every caller does, and it was
 * told a truncated document was whole.
 */
static proven_err_t writer_u8str_flush(void *ctx) {
    proven_writer_u8str_t *s = (proven_writer_u8str_t *)ctx;
    if (!s) return PROVEN_ERR_INVALID_ARG;
    return s->err;
}

proven_writer_t proven_writer_from_u8str(proven_writer_u8str_t *state, proven_u8str_t *str, proven_allocator_t alloc) {
    if (!state || !str) return (proven_writer_t){0};
    state->str = str;
    state->alloc = alloc;
    state->err = PROVEN_OK;
    return (proven_writer_t){ .ctx = state, .write_fn = writer_u8str_write, .flush_fn = writer_u8str_flush };
}

/* --- a fixed buffer ---------------------------------------------------- */

static proven_result_size_t writer_buf_write(void *ctx, proven_mem_view_t chunk) {
    proven_result_size_t res = {0};
    proven_writer_buf_t *s = (proven_writer_buf_t *)ctx;
    if (!s || !s->buf.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (s->overflowed) {
        /* Something has already been dropped. A shorter chunk that happens to fit would
         * land AFTER the hole, producing a buffer that looks like valid output and is
         * missing a piece in the middle - which nobody downstream can detect. */
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
        return res;
    }

    if (chunk.size > s->buf.size - s->len) {
        /* Refuse rather than truncate. A sink that silently drops the end of your
         * data is worse than one that says it cannot take it. Nothing is written, so
         * nothing is reported written. */
        s->overflowed = true;
        res.err = PROVEN_ERR_OUT_OF_BOUNDS;
        return res;
    }
    proven_sys_mem_copy(s->buf.ptr + s->len, chunk.ptr, chunk.size);
    s->len += chunk.size;
    res.err = PROVEN_OK;
    res.value = chunk.size;
    return res;
}

/*
 * A fixed-buffer writer that has overflowed has DROPPED data, and it must not answer a
 * flush with PROVEN_OK. There is nothing to push out - the whole point of this writer is
 * that there is no sink behind it - but "did everything I was given get through?" is the
 * question a caller asks a flush, and the honest answer here is no.
 *
 * Without this, "render, render, render, check the flush" - the shape every caller uses -
 * reported success on a buffer that had refused half the output. The write that overflowed
 * did return PROVEN_ERR_OUT_OF_BOUNDS, but a caller who checks only at the end never saw it.
 */
static proven_err_t writer_buf_flush(void *ctx) {
    proven_writer_buf_t *s = (proven_writer_buf_t *)ctx;
    if (!s) return PROVEN_ERR_INVALID_ARG;
    return s->overflowed ? PROVEN_ERR_OUT_OF_BOUNDS : PROVEN_OK;
}

proven_writer_t proven_writer_from_buffer(proven_writer_buf_t *state) {
    if (!state || !state->buf.ptr) return (proven_writer_t){0};
    return (proven_writer_t){ .ctx = state, .write_fn = writer_buf_write, .flush_fn = writer_buf_flush };
}

/* --- buffering over another writer -------------------------------------- */

/*
 * Send the buffer to the inner writer, WITHOUT flushing the inner writer. A drain because the
 * buffer is full is not a flush anyone asked for: calling the inner flush there told a console
 * writer its text had ended at an arbitrary byte, and cost a stacked buffered writer a drain of
 * its own on every one of ours.
 */
static proven_err_t writer_buffered_drain(proven_writer_buffered_t *s) {
    /* A writer that has already lost bytes cannot report success, whatever it does now.
     * The stream it was producing has a hole in it and the receiver cannot see that. */
    if (!proven_is_ok(s->err)) return s->err;

    if (s->len == 0) return PROVEN_OK;

    /*
     * Drop exactly what went out, and keep exactly what did not.
     *
     * This used to hand the whole buffer to a writer that promised all-or-nothing and,
     * on failure, keep all of it - reasoning that a failed flush must not discard
     * bytes. But a write to a pipe or a full disk really does put some bytes out and
     * THEN fail, so the next flush re-sent the whole buffer: a 6000-byte payload
     * arrived as 10,096 bytes with the first 4096 duplicated. The receiver cannot
     * detect that. Losing data is bad; silently doubling it is worse.
     */
    proven_size_t sent = 0;
    while (sent < s->len) {
        proven_mem_view_t rest = { .ptr = s->buf.ptr + sent, .size = s->len - sent };
        proven_result_size_t r = proven_writer_write_partial(s->inner, rest);
        sent += r.value;

        if (!proven_is_ok(r.err) || (r.value == 0 && rest.size > 0)) {
            /* Keep the unsent tail, and only that. */
            proven_size_t keep = s->len - sent;
            if (keep > 0 && sent > 0) proven_sys_mem_move(s->buf.ptr, s->buf.ptr + sent, keep);
            s->len = keep;
            s->err = proven_is_ok(r.err) ? PROVEN_ERR_IO : r.err;
            return s->err;
        }
    }

    s->len = 0;
    return PROVEN_OK;
}

static proven_err_t writer_buffered_flush(void *ctx) {
    proven_writer_buffered_t *s = (proven_writer_buffered_t *)ctx;
    if (!s) return PROVEN_ERR_INVALID_ARG;
    proven_err_t e = writer_buffered_drain(s);
    if (!proven_is_ok(e)) return e;
    return proven_writer_flush(s->inner);
}

static proven_result_size_t writer_buffered_write(void *ctx, proven_mem_view_t chunk) {
    proven_result_size_t res = {0};
    proven_writer_buffered_t *s = (proven_writer_buffered_t *)ctx;
    if (!s || !s->buf.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (!proven_is_ok(s->err)) {
        res.err = s->err;   /* the stream already has a hole in it */
        return res;
    }

    /* A chunk bigger than the whole buffer is passed straight through: buffering it
     * would mean either failing or splitting it, and neither helps anyone. */
    if (chunk.size >= s->buf.size) {
        res.err = writer_buffered_drain(s);
        if (!proven_is_ok(res.err)) return res;
        res = proven_writer_write_partial(s->inner, chunk);
        if (!proven_is_ok(res.err)) {
            s->err = res.err;
        } else if (res.value == 0 && chunk.size > 0) {
            /* The sink took nothing and reported no error: it has stalled. The all-or-
             * nothing wrapper turns that into PROVEN_ERR_IO for the caller, and this
             * writer must remember it - the flush path already treats {OK, 0} as a
             * failure, and the two used to disagree. Without this the writer went on
             * accepting writes, and the receiver got a stream with a hole in it. */
            s->err = PROVEN_ERR_IO;
            res.err = PROVEN_ERR_IO;
        }
        return res;
    }

    if (chunk.size > s->buf.size - s->len) {
        res.err = writer_buffered_drain(s);
        if (!proven_is_ok(res.err)) return res;
    }

    proven_sys_mem_copy(s->buf.ptr + s->len, chunk.ptr, chunk.size);
    s->len += chunk.size;
    res.err = PROVEN_OK;
    res.value = chunk.size;
    return res;
}

proven_writer_t proven_writer_buffered(proven_writer_buffered_t *state, proven_writer_t inner, proven_mem_mut_t buf) {
    if (!state || !buf.ptr || buf.size == 0 || !proven_writer_is_valid(inner)) {
        return (proven_writer_t){0};
    }
    state->inner = inner;
    state->buf = buf;
    state->len = 0;
    state->err = PROVEN_OK;
    return (proven_writer_t){ .ctx = state, .write_fn = writer_buffered_write, .flush_fn = writer_buffered_flush };
}

// -------------------------------------------------------------
// Reader
// -------------------------------------------------------------

proven_result_size_t proven_reader_read(proven_reader_t r, proven_mem_mut_t dest) {
    proven_result_size_t res = {0};
    if (!proven_reader_is_valid(r)) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    if (dest.size == 0) return res;   /* OK, zero bytes */
    if (!dest.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    return r.read_fn(r.ctx, dest);
}

/* --- a file ------------------------------------------------------------ */

static proven_result_size_t reader_file_read(void *ctx, proven_mem_mut_t dest) {
    proven_result_size_t res = {0};
    proven_file_t *file = (proven_file_t *)ctx;
    if (!file) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    return proven_fs_read(*file, dest);
}

proven_reader_t proven_reader_from_file(proven_file_t *file) {
    if (!file) return (proven_reader_t){0};
    return (proven_reader_t){ .ctx = file, .read_fn = reader_file_read };
}

/* --- bytes you already have --------------------------------------------- */

static proven_result_size_t reader_view_read(void *ctx, proven_mem_mut_t dest) {
    proven_result_size_t res = {0};
    proven_reader_view_t *s = (proven_reader_view_t *)ctx;
    if (!s) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }
    proven_size_t left = s->view.size - s->cursor;
    if (left == 0) {
        res.err = PROVEN_ERR_EOF;
        return res;
    }
    proven_size_t n = left < dest.size ? left : dest.size;
    proven_sys_mem_copy(dest.ptr, s->view.ptr + s->cursor, n);
    s->cursor += n;
    res.err = PROVEN_OK;
    res.value = n;
    return res;
}

proven_reader_t proven_reader_from_view(proven_reader_view_t *state, proven_u8str_view_t view) {
    if (!state) return (proven_reader_t){0};
    if (view.size > 0 && !view.ptr) {
        view = (proven_u8str_view_t){ .ptr = (const proven_byte_t *)0, .size = 0 };
    }
    state->view = view;
    state->cursor = 0;
    return (proven_reader_t){ .ctx = state, .read_fn = reader_view_read };
}

/* --- buffering over another reader --------------------------------------- */

/* Pull more bytes in, compacting whatever has not been handed out yet to the front.
 * Returns false when the source is exhausted and nothing new arrived. */
static bool reader_buffered_fill(proven_reader_buffered_t *s) {
    if (s->eof || !proven_is_ok(s->err)) return false;

    if (s->cursor > 0) {
        proven_size_t keep = s->len - s->cursor;
        if (keep > 0) proven_sys_mem_move(s->buf.ptr, s->buf.ptr + s->cursor, keep);
        s->len = keep;
        s->cursor = 0;
    }
    if (s->len == s->buf.size) return false;   /* full, and nothing consumed */

    /* A byte the line reader looked ahead at belongs to the stream: put it back before asking
     * the source for more, or it is silently dropped. Re-inserting it is itself PROGRESS - the
     * buffer is now non-empty - which is why `made_progress` below tracks the buffer, not just
     * what the source hands over. */
    bool reinserted_peek = false;
    if (s->has_peek) {
        s->buf.ptr[s->len++] = s->peek;
        s->has_peek = false;
        reinserted_peek = true;
        if (s->len == s->buf.size) return true;
    }

    proven_mem_mut_t space = { .ptr = s->buf.ptr + s->len, .size = s->buf.size - s->len };
    proven_result_size_t r = proven_reader_read(s->inner, space);
    if (r.err == PROVEN_ERR_EOF || (proven_is_ok(r.err) && r.value == 0)) {
        /* An EOF may still carry bytes - proven_sys_io_read_all returns {EOF, N} with N
         * nonzero, and any reader a caller writes may do the same. Dropping them here made
         * the last line of a file disappear whenever the source reported its end and its
         * final bytes in the same breath, and it made this reader disagree with the
         * buffered scanner in the same library, which honours that shape. */
        s->len += r.value;
        s->eof = true;
        /* True if the buffer gained a byte this call - from the source OR from a re-inserted
         * peek. Returning `r.value > 0` alone stranded the peek byte: at EOF the source
         * contributes nothing, so a caller reading raw bytes after a too-long line got a
         * spurious EOF with the peeked byte still unread, and a read-to-EOF loop never came
         * back for it. */
        return r.value > 0 || reinserted_peek;
    }
    if (!proven_is_ok(r.err)) {
        /*
         * An I/O failure is NOT end of input, and reporting it as one is how a disk
         * error that truncated a file becomes indistinguishable from a complete file:
         * the caller reads two lines, gets EOF, and believes it has the whole thing.
         *
         * Remember the failure so every subsequent call reports it instead.
         */
        s->err = r.err;
        return false;
    }
    s->len += r.value;
    return true;
}

static proven_result_size_t reader_buffered_read(void *ctx, proven_mem_mut_t dest) {
    proven_result_size_t res = {0};
    proven_reader_buffered_t *s = (proven_reader_buffered_t *)ctx;
    if (!s || !s->buf.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    if (s->cursor == s->len && !reader_buffered_fill(s)) {
        res.err = proven_is_ok(s->err) ? PROVEN_ERR_EOF : s->err;
        return res;
    }

    proven_size_t have = s->len - s->cursor;
    proven_size_t n = have < dest.size ? have : dest.size;
    proven_sys_mem_copy(dest.ptr, s->buf.ptr + s->cursor, n);
    s->cursor += n;
    s->scanned = s->scanned > n ? s->scanned - n : 0;   /* still relative to the cursor */
    res.err = PROVEN_OK;
    res.value = n;
    return res;
}

proven_reader_t proven_reader_buffered(proven_reader_buffered_t *state, proven_reader_t inner, proven_mem_mut_t buf) {
    if (!state || !buf.ptr || buf.size == 0 || !proven_reader_is_valid(inner)) {
        return (proven_reader_t){0};
    }
    state->inner = inner;
    state->buf = buf;
    state->len = 0;
    state->cursor = 0;
    state->eof = false;
    state->err = PROVEN_OK;
    state->peek = 0;
    state->has_peek = false;   /* a stack-declared state holds garbage here otherwise */
    state->scanned = 0;
    return (proven_reader_t){ .ctx = state, .read_fn = reader_buffered_read };
}

proven_result_u8str_view_t proven_reader_read_line(proven_reader_buffered_t *s) {
    proven_result_u8str_view_t res = {0};
    if (!s || !s->buf.ptr) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    for (;;) {
        /* Is there a newline in what we already hold? Search only the bytes not searched
         * before (RFC-0009 P-104): the byte loop restarted at the cursor after every refill,
         * so a 16 KB line arriving a byte at a time cost 4.8 us per byte. */
        proven_size_t from = s->cursor + s->scanned;
        if (from > s->len) from = s->len;   /* a caller-zeroed or older state */
        const proven_byte_t *nl = (const proven_byte_t *)proven_sys_mem_chr(s->buf.ptr + from, '\n', s->len - from);
        if (nl) {
            proven_size_t i = (proven_size_t)(nl - s->buf.ptr);
            proven_size_t end = i;
            if (end > s->cursor && s->buf.ptr[end - 1] == (proven_byte_t)'\r') --end;

            res.err = PROVEN_OK;
            res.val = (proven_u8str_view_t){ .ptr = s->buf.ptr + s->cursor, .size = end - s->cursor };
            s->cursor = i + 1;   /* step over the newline */
            s->scanned = 0;
            return res;
        }
        s->scanned = s->len - s->cursor;   /* all of it searched; compaction keeps this valid */

        /* No newline yet.
         *
         * Ask FIRST whether the buffer is simply full, because reader_buffered_fill
         * cannot tell us apart from a source that ended: it returns false for both.
         * Confusing the two is how a too-long line gets handed back truncated - as a
         * successful read of a line that was never a line - which is a corruption the
         * caller has no way to detect.
         *
         * cursor == 0 after a compaction means everything held is one unterminated
         * line, and there is nowhere left to put more of it. */
        if (s->cursor == 0 && s->len == s->buf.size) {
            /*
             * The buffer is full and holds no newline. That does NOT yet mean the line is too
             * long. It means one of three things, and only one of them is an error:
             *
             *   - the next byte is a newline     -> the line is EXACTLY buffer-sized, and done;
             *   - the source has ended           -> what we hold IS the file's final line;
             *   - the next byte is anything else -> the line really is longer than the buffer.
             *
             * Answering "too long" without looking cost real data: a 4-byte file with no
             * trailing newline, read through a 4-byte buffer, came back OUT_OF_BOUNDS with its
             * entire contents unreachable. One byte of lookahead tells the three apart, and it
             * makes the documented rule - only a LONGER line is an error - the rule that is
             * actually enforced.
             */
            proven_byte_t probe = 0;
            proven_result_size_t pr = { .err = PROVEN_OK, .value = 0 };

            if (s->has_peek) {
                probe = s->peek;
                pr.value = 1;
            } else if (s->eof) {
                pr.value = 0;   /* already known to be exhausted */
            } else {
                pr = proven_reader_read(s->inner, (proven_mem_mut_t){ .ptr = &probe, .size = 1 });
                if (pr.err == PROVEN_ERR_EOF && pr.value == 0) s->eof = true;
            }

            if (!proven_is_ok(pr.err) && pr.err != PROVEN_ERR_EOF) {
                /* The source broke. What we hold is a fragment, not a line. */
                s->err = pr.err;
                res.err = pr.err;
                return res;
            }

            if (pr.value == 0) {
                /* The source has ended: the full buffer is the final, unterminated line. */
                s->eof = true;
                res.err = PROVEN_OK;
                res.val = (proven_u8str_view_t){ .ptr = s->buf.ptr, .size = s->len };
                s->cursor = s->len;
                s->scanned = 0;
                return res;
            }

            if (probe == (proven_byte_t)'\n') {
                /* The line is exactly buffer-sized and properly terminated. */
                proven_size_t end = s->len;
                if (end > 0 && s->buf.ptr[end - 1] == (proven_byte_t)'\r') --end;

                res.err = PROVEN_OK;
                res.val = (proven_u8str_view_t){ .ptr = s->buf.ptr, .size = end };
                s->cursor = s->len;   /* the newline itself is consumed, not stored */
                s->scanned = 0;
                s->has_peek = false;
                return res;
            }

            /* The line really is longer than the buffer. Keep the byte we looked at - it is
             * part of the stream, and dropping it would corrupt whatever comes next. */
            s->peek = probe;
            s->has_peek = true;
            res.err = PROVEN_ERR_OUT_OF_BOUNDS;
            return res;
        }

        if (!reader_buffered_fill(s)) {
            /* The source really is exhausted. A final line with no trailing newline
             * is still a line - dropping it is how the last record of a file goes
             * missing. */
            /* If the source BROKE rather than ended, say so - what we hold is a
             * fragment of a line, not a line. */
            if (!proven_is_ok(s->err)) {
                res.err = s->err;
                return res;
            }
            if (s->cursor < s->len) {
                res.err = PROVEN_OK;
                res.val = (proven_u8str_view_t){ .ptr = s->buf.ptr + s->cursor, .size = s->len - s->cursor };
                s->cursor = s->len;
                s->scanned = 0;
                return res;
            }
            res.err = PROVEN_ERR_EOF;
            return res;
        }
    }
}

// -------------------------------------------------------------
// UTF-16 text
// -------------------------------------------------------------

#ifndef PROVEN_NO_U16STR

static bool text_enc_is_concrete(proven_text_encoding_t enc) {
    return enc == PROVEN_TEXT_UTF8 || enc == PROVEN_TEXT_UTF16LE || enc == PROVEN_TEXT_UTF16BE;
}

proven_err_t proven_writer_write_bom(proven_writer_t w, proven_text_encoding_t enc) {
    static const proven_byte_t bom8[] = { 0xEF, 0xBB, 0xBF };
    static const proven_byte_t bomle[] = { 0xFF, 0xFE };
    static const proven_byte_t bombe[] = { 0xFE, 0xFF };
    switch (enc) {
    case PROVEN_TEXT_UTF8:    return proven_writer_write(w, (proven_mem_view_t){ bom8, sizeof bom8 });
    case PROVEN_TEXT_UTF16LE: return proven_writer_write(w, (proven_mem_view_t){ bomle, sizeof bomle });
    case PROVEN_TEXT_UTF16BE: return proven_writer_write(w, (proven_mem_view_t){ bombe, sizeof bombe });
    default:                  return PROVEN_ERR_INVALID_ARG;
    }
}

proven_err_t proven_writer_write_u16(proven_writer_t w, proven_u16str_view_t text, proven_text_encoding_t enc) {
    if (!proven_writer_is_valid(w) || !text_enc_is_concrete(enc)) return PROVEN_ERR_INVALID_ARG;
    if (text.size > 0 && !text.ptr) return PROVEN_ERR_INVALID_ARG;

    /* Validate the whole text first: malformed text must write nothing, and a writer cannot
     * take back what it has already sent. */
    proven_result_size_t need = proven_utf16_to_utf8_size(text.ptr, text.size);
    if (!proven_is_ok(need.err)) return need.err;

    proven_byte_t chunk[256];
    proven_size_t done = 0;
    while (done < text.size) {
        proven_size_t n_bytes, n_units;
        if (enc == PROVEN_TEXT_UTF8) {
            proven_utf_step_t st = proven_utf16_to_utf8_partial(text.ptr + done, text.size - done, chunk, sizeof chunk);
            if (st.err != PROVEN_OK && st.err != PROVEN_ERR_OUT_OF_BOUNDS) return st.err;
            n_bytes = st.written;
            n_units = st.consumed;
        } else {
            /* Byte order is decided here, by shifting, never by the host's own layout. */
            n_units = text.size - done;
            if (n_units > sizeof chunk / 2) n_units = sizeof chunk / 2;
            for (proven_size_t i = 0; i < n_units; ++i) {
                proven_u16 u = text.ptr[done + i];
                proven_byte_t hi = (proven_byte_t)(u >> 8), lo = (proven_byte_t)(u & 0xFFu);
                chunk[2 * i]     = (enc == PROVEN_TEXT_UTF16LE) ? lo : hi;
                chunk[2 * i + 1] = (enc == PROVEN_TEXT_UTF16LE) ? hi : lo;
            }
            n_bytes = n_units * 2;
        }
        proven_err_t e = proven_writer_write(w, (proven_mem_view_t){ chunk, n_bytes });
        if (!proven_is_ok(e)) return e;
        done += n_units;
    }
    return PROVEN_OK;
}

proven_err_t proven_u16_reader_init(proven_u16_reader_t *st, proven_reader_t inner, proven_text_encoding_t enc,
                                    proven_u16 *buf, proven_size_t cap) {
    if (!st || !proven_reader_is_valid(inner) || !buf || cap < 2) return PROVEN_ERR_INVALID_ARG;
    if (!text_enc_is_concrete(enc) && enc != PROVEN_TEXT_AUTO) return PROVEN_ERR_INVALID_ARG;
    *st = (proven_u16_reader_t){0};
    st->inner = inner;
    st->enc = enc;
    st->buf = buf;
    st->cap = cap;
    st->err = PROVEN_OK;
    st->bom_checked = (enc != PROVEN_TEXT_AUTO);
    return PROVEN_OK;
}

/* Top up the raw staging area. False when the source is exhausted or broke (st->err says).
 *
 * 1 KiB, and consumed bytes are skipped with a cursor rather than moved after every decode: a
 * 64-byte area cost one read of the source per 64 bytes - some 16,000 read() calls for a 1 MiB
 * file through sysio, against 256 for the byte line reader (code review). The tail is moved to
 * the front only here, once per refill. */
static bool u16r_read_raw(proven_u16_reader_t *st) {
    if (st->eof) return false;
    if (st->raw_pos > 0) {
        proven_size_t keep = st->raw_len - st->raw_pos;
        if (keep > 0) proven_sys_mem_move(st->raw, st->raw + st->raw_pos, keep);
        st->raw_len = keep;
        st->raw_pos = 0;
    }
    if (st->raw_len == sizeof st->raw) return true;
    proven_result_size_t r = proven_reader_read(st->inner,
        (proven_mem_mut_t){ st->raw + st->raw_len, sizeof st->raw - st->raw_len });
    st->raw_len += r.value;
    if (r.err == PROVEN_ERR_EOF || (proven_is_ok(r.err) && r.value == 0)) {
        st->eof = true;
        return r.value > 0;
    }
    if (!proven_is_ok(r.err)) {
        st->err = r.err;
        return false;
    }
    return true;
}

/* Decode the staged bytes into dest. `consumed` is in bytes, `written` in units; the stop
 * reasons are utf.h's. */
static proven_utf_step_t u16r_decode_raw(proven_u16_reader_t *st, proven_u16 *dest, proven_size_t cap) {
    if (st->enc == PROVEN_TEXT_UTF8) {
        return proven_utf8_to_utf16_partial((proven_u8str_view_t){ st->raw + st->raw_pos, st->raw_len - st->raw_pos }, dest, cap);
    }

    proven_utf_step_t out = { PROVEN_OK, 0, 0 };
    proven_u16 units[sizeof ((proven_u16_reader_t *)0)->raw / 2];
    const proven_byte_t *raw = st->raw + st->raw_pos;
    proven_size_t raw_n = st->raw_len - st->raw_pos;
    proven_size_t n = raw_n / 2;
    /* Decode and validate only what this call can hand out, plus one unit to see whether the
     * last one taken is half of a pair (RFC-0009 P-103). The whole staged area - up to 512
     * units - used to be validated on every call, so reading with cap 2 cost 261 ns a unit. */
    proven_size_t m = n;
    bool more_staged = false;
    if (cap < m) {
        m = cap + 1;
        more_staged = true;
    }
    for (proven_size_t i = 0; i < m; ++i) {
        proven_byte_t a = raw[2 * i], b = raw[2 * i + 1];
        units[i] = (st->enc == PROVEN_TEXT_UTF16LE) ? (proven_u16)(a | (b << 8)) : (proven_u16)((a << 8) | b);
    }
    /* Validate as whole characters by converting to UTF-8 into scratch nobody reads: this is
     * the one place that already knows a lone surrogate from half a pair. */
    proven_byte_t scratch[sizeof units / sizeof units[0] * 3];
    proven_utf_step_t v = proven_utf16_to_utf8_partial(units, m, scratch, sizeof scratch);
    proven_size_t take = v.consumed;
    if (take > cap) {
        take = cap;
        if (take > 0 && units[take - 1] >= 0xD800u && units[take - 1] <= 0xDBFFu) --take;
        out.err = PROVEN_ERR_OUT_OF_BOUNDS;
    } else if (v.err == PROVEN_ERR_INVALID_ENCODING) {
        out.err = PROVEN_ERR_INVALID_ENCODING;
    } else if (v.consumed < m && more_staged) {
        /* The unit after the last one examined is staged: a high surrogate at the end of the
         * window has its partner there, so this is "no room", not "need more input". */
        out.err = PROVEN_ERR_OUT_OF_BOUNDS;
    } else if (v.consumed < m || (raw_n & 1u)) {
        out.err = PROVEN_ERR_NEED_MORE;   /* a trailing high surrogate, or an odd byte */
    }
    for (proven_size_t i = 0; i < take; ++i) dest[i] = units[i];
    out.written = take;
    out.consumed = take * 2;
    return out;
}

static void u16r_check_bom(proven_u16_reader_t *st) {
    while (st->raw_len - st->raw_pos < 3 && u16r_read_raw(st)) {}
    const proven_byte_t *r = st->raw + st->raw_pos;
    proven_size_t avail = st->raw_len - st->raw_pos;
    proven_size_t skip = 0;
    if (avail >= 3 && r[0] == 0xEF && r[1] == 0xBB && r[2] == 0xBF) { st->enc = PROVEN_TEXT_UTF8; skip = 3; }
    else if (avail >= 2 && r[0] == 0xFF && r[1] == 0xFE)            { st->enc = PROVEN_TEXT_UTF16LE; skip = 2; }
    else if (avail >= 2 && r[0] == 0xFE && r[1] == 0xFF)            { st->enc = PROVEN_TEXT_UTF16BE; skip = 2; }
    else                                                                  { st->enc = PROVEN_TEXT_UTF8; }
    st->raw_pos += skip;
    st->bom_checked = true;
}

/*
 * Decode at least one whole character into dest, or say why not: OK with units, EOF, or the
 * sticky error. OUT_OF_BOUNDS with nothing written means the next character is a surrogate
 * pair and `cap` is 1.
 */
static proven_result_size_t u16r_decode(proven_u16_reader_t *st, proven_u16 *dest, proven_size_t cap) {
    proven_result_size_t res = { PROVEN_OK, 0 };
    if (!proven_is_ok(st->err)) { res.err = st->err; return res; }
    if (st->peek_len > 0) {
        if (cap < st->peek_len) { res.err = PROVEN_ERR_OUT_OF_BOUNDS; return res; }
        for (proven_size_t i = 0; i < st->peek_len; ++i) dest[i] = st->peek[i];
        res.value = st->peek_len;
        st->peek_len = 0;
        return res;
    }
    if (!st->bom_checked) {
        u16r_check_bom(st);
        if (!proven_is_ok(st->err)) { res.err = st->err; return res; }
    }

    for (;;) {
        proven_utf_step_t d = u16r_decode_raw(st, dest, cap);
        st->raw_pos += d.consumed;
        if (d.written > 0) { res.value = d.written; return res; }
        if (d.err == PROVEN_ERR_INVALID_ENCODING) {
            st->err = PROVEN_ERR_INVALID_ENCODING;
            res.err = st->err;
            return res;
        }
        if (d.err == PROVEN_ERR_OUT_OF_BOUNDS) { res.err = PROVEN_ERR_OUT_OF_BOUNDS; return res; }

        /* Nothing decodable is staged: fetch more, or finish. */
        if (!u16r_read_raw(st)) {
            if (!proven_is_ok(st->err)) { res.err = st->err; return res; }
            if (st->raw_len > st->raw_pos) {
                /* The source ended inside a character. */
                st->err = PROVEN_ERR_INVALID_ENCODING;
                res.err = st->err;
                return res;
            }
            res.err = PROVEN_ERR_EOF;
            return res;
        }
    }
}

proven_result_u16str_view_t proven_u16_reader_read_line(proven_u16_reader_t *st) {
    proven_result_u16str_view_t res = {0};
    if (!st || !st->buf) { res.err = PROVEN_ERR_INVALID_ARG; return res; }

    for (;;) {
        for (proven_size_t i = st->cursor; i < st->len; ++i) {
            if (st->buf[i] != (proven_u16)'\n') continue;
            proven_size_t end = i;
            if (end > st->cursor && st->buf[end - 1] == (proven_u16)'\r') --end;
            res.val = (proven_u16str_view_t){ st->buf + st->cursor, end - st->cursor };
            st->cursor = i + 1;
            return res;
        }

        if (st->cursor > 0) {
            proven_size_t keep = st->len - st->cursor;
            if (keep > 0) proven_sys_mem_move(st->buf, st->buf + st->cursor, keep * sizeof(proven_u16));
            st->len = keep;
            st->cursor = 0;
        }

        proven_result_size_t r = { PROVEN_ERR_OUT_OF_BOUNDS, 0 };
        if (st->len < st->cap) r = u16r_decode(st, st->buf + st->len, st->cap - st->len);

        if (r.err == PROVEN_ERR_OUT_OF_BOUNDS) {
            /* No room for the next character. As in proven_reader_read_line, that is an error
             * only if the line goes on: look at one more character first. A newline means the
             * line exactly fills the buffer; the end of the text means it is the last line. */
            if (st->peek_len == 0) {
                /* Exactly one character: one unit, or two if it is a surrogate pair. A single
                 * decode into two units could take the newline AND the next line's first
                 * character, and the newline would no longer be the whole peek. */
                proven_result_size_t p = u16r_decode(st, st->peek, 1);
                if (p.err == PROVEN_ERR_OUT_OF_BOUNDS) p = u16r_decode(st, st->peek, 2);
                if (p.err == PROVEN_ERR_EOF) {
                    res.val = (proven_u16str_view_t){ st->buf, st->len };
                    st->cursor = st->len;
                    return res;
                }
                if (!proven_is_ok(p.err)) { res.err = p.err; return res; }
                st->peek_len = p.value;
            }
            if (st->peek_len == 1 && st->peek[0] == (proven_u16)'\n') {
                proven_size_t end = st->len;
                if (end > 0 && st->buf[end - 1] == (proven_u16)'\r') --end;
                res.val = (proven_u16str_view_t){ st->buf, end };
                st->cursor = st->len;
                st->peek_len = 0;
                return res;
            }
            res.err = PROVEN_ERR_OUT_OF_BOUNDS;
            return res;
        }
        if (r.err == PROVEN_ERR_EOF) {
            if (st->len > 0) {
                res.val = (proven_u16str_view_t){ st->buf, st->len };
                st->cursor = st->len;
                return res;
            }
            res.err = PROVEN_ERR_EOF;
            return res;
        }
        if (!proven_is_ok(r.err)) { res.err = r.err; return res; }
        st->len += r.value;
    }
}

proven_result_size_t proven_u16_reader_read(proven_u16_reader_t *st, proven_u16 *dest, proven_size_t cap) {
    proven_result_size_t res = { PROVEN_OK, 0 };
    if (!st || !st->buf || !dest || cap < 2) { res.err = PROVEN_ERR_INVALID_ARG; return res; }

    if (st->cursor < st->len) {
        proven_size_t n = st->len - st->cursor;
        if (n > cap) {
            n = cap;
            proven_u16 last = st->buf[st->cursor + n - 1];
            if (last >= 0xD800u && last <= 0xDBFFu) --n;   /* never half a pair */
        }
        for (proven_size_t i = 0; i < n; ++i) dest[i] = st->buf[st->cursor + i];
        st->cursor += n;
        res.value = n;
        return res;
    }
    return u16r_decode(st, dest, cap);
}

#endif /* PROVEN_NO_U16STR */

// -------------------------------------------------------------
// Formatting straight into a writer
// -------------------------------------------------------------

proven_fmt_result_t proven_fmt_to_writer_impl(proven_writer_t w, proven_mem_mut_t scratch,
                                              const char *fmt, const proven_arg_t *args, proven_size_t args_count) {
    proven_fmt_result_t res = {0};
    if (!proven_writer_is_valid(w) || !scratch.ptr || scratch.size == 0 || !fmt) {
        res.err = PROVEN_ERR_INVALID_ARG;
        return res;
    }

    /* Borrowed over the caller's memory: this allocates nothing, and the atomic
     * (non-truncating) mode means a line that does not fit is refused rather than
     * cut in half. A reader must never see half a line. */
    proven_u8str_t s = proven_u8str_borrow(scratch.ptr, scratch.size);
    res = proven_u8str_fmt_internal((proven_allocator_t){0}, &s, false, fmt,
                                    (proven_allocator_t){0}, args, args_count);
    if (!proven_is_ok(res.err)) return res;

    proven_err_t werr = proven_writer_write_str(w, proven_u8str_as_view(&s));
    if (!proven_is_ok(werr)) res.err = werr;
    return res;
}
