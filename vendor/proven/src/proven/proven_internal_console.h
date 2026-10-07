#ifndef PROVEN_INTERNAL_CONSOLE_H
#define PROVEN_INTERNAL_CONSOLE_H

#include "proven/sysio.h"
#include "proven/utf.h"
#include "../../platform/proven_sys_mem.h"

/*
 * UTF-8 in and out of a console that speaks UTF-16 (B-039).
 *
 * The program writes and reads UTF-8, in pieces whose boundaries have nothing to do with
 * characters: a buffered writer flushes at 4096 bytes, a line reader asks for whatever room its
 * buffer has left. The console takes and gives UTF-16 code units. These two functions sit
 * between, and their whole difficulty is the pieces:
 *
 *   - writing, a UTF-8 character can arrive split across two writes. Its first bytes are
 *     carried in `carry` and completed by the next write - never dropped, never replaced;
 *   - reading, one console unit can become up to three bytes (four for a pair) that do not fit
 *     in what the caller asked for. The rest is carried and handed out first next time. A high
 *     surrogate at the end of a console read waits for its low half.
 *
 * Strict, like utf.h: malformed UTF-8 is refused with PROVEN_ERR_INVALID_ENCODING after the
 * valid text before it has gone out, and so is a lone surrogate from the console.
 *
 * Header-only and driven through callbacks, so tests can run it against a fake console on any
 * host (tests/test_unit_sysio_console.c); sysio.c binds it to the PAL's WriteConsoleW and
 * ReadConsoleW.
 */

typedef struct {
    void *ctx;
    /* Write all `n` units, or fail; `value` is how many went out either way. */
    proven_result_size_t (*write_units)(void *ctx, const proven_u16 *units, proven_size_t n);
    /* Read up to `cap` units. PROVEN_ERR_EOF for nothing. */
    proven_result_size_t (*read_units)(void *ctx, proven_u16 *units, proven_size_t cap);
} proven_console_io_t;

#define PROVEN_CONSOLE_UNITS 256

/* Send units; on a failure report the UTF-8 bytes that the units which DID go out came from. */
static inline proven_result_size_t proven_console_send(proven_console_io_t io, const proven_u16 *u, proven_size_t n,
                                                       proven_size_t bytes_before) {
    proven_result_size_t r = io.write_units(io.ctx, u, n);
    if (proven_is_ok(r.err)) return (proven_result_size_t){ PROVEN_OK, bytes_before };
    /* Count whole characters only: the console may have taken the high half of a pair. */
    proven_size_t whole = r.value;
    if (whole > n) whole = n;
    if (whole > 0 && u[whole - 1] >= 0xD800u && u[whole - 1] <= 0xDBFFu) --whole;
    proven_result_size_t went = proven_utf16_to_utf8_size(u, whole);
    return (proven_result_size_t){ r.err, bytes_before + (proven_is_ok(went.err) ? went.value : 0) };
}

/*
 * Write UTF-8 bytes to the console. `value` is the input consumed, which counts bytes held in
 * the carry: they are this writer's responsibility now, and proven_console_finish reports them
 * if the text ends before they are completed.
 */
static inline proven_result_size_t proven_console_write_utf8(proven_console_io_t io, proven_sysio_carry_t *c,
                                                             const proven_byte_t *p, proven_size_t n) {
    proven_u16 units[PROVEN_CONSOLE_UNITS];
    proven_size_t used = 0;

    /* First finish the character the last write left open, a byte at a time. */
    while (c->len > 0) {
        if (used == n) return (proven_result_size_t){ PROVEN_OK, n };
        c->bytes[c->len++] = p[used++];
        proven_utf_step_t st = proven_utf8_to_utf16_partial((proven_u8str_view_t){ c->bytes, c->len }, units, 2);
        if (st.err == PROVEN_ERR_NEED_MORE) continue;
        c->len = 0;
        if (st.err != PROVEN_OK) return (proven_result_size_t){ st.err, used - 1 };
        proven_result_size_t r = proven_console_send(io, units, st.written, used);
        if (!proven_is_ok(r.err)) return (proven_result_size_t){ r.err, used - 1 };
    }

    while (used < n) {
        proven_utf_step_t st = proven_utf8_to_utf16_partial((proven_u8str_view_t){ p + used, n - used },
                                                            units, PROVEN_CONSOLE_UNITS);
        if (st.written > 0) {
            proven_result_size_t r = proven_console_send(io, units, st.written, used);
            if (!proven_is_ok(r.err)) return r;
        }
        used += st.consumed;
        if (st.err == PROVEN_OK || st.err == PROVEN_ERR_OUT_OF_BOUNDS) continue;
        if (st.err == PROVEN_ERR_NEED_MORE) {
            /* At most three bytes: a fourth would have completed the character. */
            proven_size_t tail = n - used;
            for (proven_size_t i = 0; i < tail; ++i) c->bytes[i] = p[used + i];
            c->len = (proven_u8)tail;
            return (proven_result_size_t){ PROVEN_OK, n };
        }
        return (proven_result_size_t){ st.err, used };
    }
    return (proven_result_size_t){ PROVEN_OK, n };
}

/*
 * A writer's flush is NOT the end of the text: a buffered writer drains at its buffer size, and a
 * caller may flush between any two bytes. So flush keeps a character that is still open, and the
 * next write completes it - or refuses it, if what follows cannot. (It used to call finish, and
 * valid UTF-8 split across a buffer boundary came back INVALID_ENCODING with the carried bytes
 * dropped; found by code review, reproduced in tests/test_unit_sysio_console.)
 */
static inline proven_err_t proven_console_flush(proven_sysio_carry_t *c) {
    (void)c;
    return PROVEN_OK;
}

/* The text is over - a whole formatted string has been written. A character still open is a
 * character that never arrived. */
static inline proven_err_t proven_console_finish(proven_sysio_carry_t *c) {
    if (c->len == 0) return PROVEN_OK;
    c->len = 0;
    return PROVEN_ERR_INVALID_ENCODING;
}

/* Hand out converted bytes the last read could not fit. */
static inline proven_result_size_t proven_console_drain(proven_sysio_carry_t *c, proven_byte_t *dest, proven_size_t cap) {
    proven_size_t n = c->len < cap ? c->len : cap;
    for (proven_size_t i = 0; i < n; ++i) dest[i] = c->bytes[i];
    for (proven_size_t i = n; i < c->len; ++i) c->bytes[i - n] = c->bytes[i];
    c->len = (proven_u8)(c->len - n);
    return (proven_result_size_t){ PROVEN_OK, n };
}

/*
 * Read console text as UTF-8 into `dest`. Ctrl+Z (U+001A) as the first unit of a read is the
 * console's end of input, as it is for every Windows console program.
 */
static inline proven_result_size_t proven_console_read_utf8(proven_console_io_t io, proven_sysio_carry_t *c,
                                                            proven_byte_t *dest, proven_size_t cap) {
    if (cap == 0) return (proven_result_size_t){ PROVEN_OK, 0 };
    if (c->len > 0) return proven_console_drain(c, dest, cap);
    if (c->broken) return (proven_result_size_t){ PROVEN_ERR_INVALID_ENCODING, 0 };

    /* Ask for no more units than can land in `dest` - three bytes each at most - so that
     * what does not fit is never more than the one character the carry can hold. */
    proven_u16 units[128];
    for (;;) {
        proven_size_t have = 0;
        if (c->has_high) {
            units[have++] = c->high;
            c->has_high = false;
        }
        proven_size_t ask = cap / 3;
        if (ask < 1) ask = 1;
        if (ask > sizeof units / sizeof units[0] - have) ask = sizeof units / sizeof units[0] - have;

        proven_result_size_t r = io.read_units(io.ctx, units + have, ask);
        if (r.err == PROVEN_ERR_EOF || (proven_is_ok(r.err) && r.value == 0)) {
            /* A high surrogate and then nothing: half a character. */
            return (proven_result_size_t){ have ? PROVEN_ERR_INVALID_ENCODING : PROVEN_ERR_EOF, 0 };
        }
        if (!proven_is_ok(r.err)) {
            if (have) { c->high = units[0]; c->has_high = true; }
            return (proven_result_size_t){ r.err, 0 };
        }
        have += r.value;
        if (units[0] == 0x001Au) return (proven_result_size_t){ PROVEN_ERR_EOF, 0 };

        /* A pair split by the read: keep its high half for the next one. */
        if (units[have - 1] >= 0xD800u && units[have - 1] <= 0xDBFFu) {
            c->high = units[have - 1];
            c->has_high = true;
            --have;
            if (have == 0) continue;   /* only the high half came: read its partner now */
        }

        proven_utf_step_t st = proven_utf16_to_utf8_partial(units, have, dest, cap);
        if (st.err == PROVEN_ERR_INVALID_ENCODING) {
            /* Hand out the valid text first; the refusal comes with the next read. */
            if (st.written == 0) return (proven_result_size_t){ st.err, 0 };
            c->broken = true;
            return (proven_result_size_t){ PROVEN_OK, st.written };
        }
        if (st.consumed < have) {
            /* The rest did not fit: one character at most (see `ask`), into the carry. */
            proven_utf_step_t rest = proven_utf16_to_utf8_partial(units + st.consumed, have - st.consumed,
                                                                  c->bytes, sizeof c->bytes);
            c->len = (proven_u8)rest.written;
            if (rest.err != PROVEN_OK || rest.consumed < have - st.consumed) c->broken = true;
            /* `dest` was too small for even the first character: start handing it out. */
            if (st.written == 0 && c->len > 0) return proven_console_drain(c, dest, cap);
        }
        /* Never a zero-byte success: a reader that answers "nothing, and fine" loops forever. */
        if (st.written == 0) return (proven_result_size_t){ PROVEN_ERR_INVALID_ENCODING, 0 };
        return (proven_result_size_t){ PROVEN_OK, st.written };
    }
}

#endif /* PROVEN_INTERNAL_CONSOLE_H */
