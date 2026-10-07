#ifndef PROVEN_UTF_H
#define PROVEN_UTF_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/allocator.h"
#include "proven/u8str.h"
#ifndef PROVEN_NO_U16STR
#include "proven/u16str.h"
#endif

/**
 * @file utf.h
 * @brief UTF-8 to UTF-16 and back, strictly.
 *
 * Windows speaks UTF-16; files, pipes and the rest of this library speak UTF-8. Before this
 * header there was no function that crossed between them, so every caller who needed a wide
 * string wrote one - and the one in `proven_time_u16_fmt` widened each UTF-8 BYTE into a code
 * unit, which is right for ASCII and mojibake for everything else.
 *
 * **Strict, everywhere.** Malformed input is refused with PROVEN_ERR_INVALID_ENCODING, never
 * repaired: no U+FFFD substitution, no skipping. Refused are overlong UTF-8 forms, encoded
 * surrogates (U+D800..U+DFFF), anything above U+10FFFF, stray continuation bytes, and in UTF-16
 * a low surrogate with no high one before it or a high surrogate followed by anything but a low
 * one. A converter that quietly repairs text is a converter that quietly changes it.
 *
 * **Malformed and incomplete are different answers.** Text read in pieces - from a pipe, a
 * buffered writer, a console - can end in the middle of a character that the next piece
 * completes. The `_partial` functions stop there with PROVEN_ERR_NEED_MORE and tell you how
 * much they consumed, so you can keep the tail and try again with more. The whole-text
 * functions have no "more" to wait for, so a text that ends mid-character is malformed.
 *
 * Pure computation: no allocation except in the `_grow` forms, no OS, freestanding-available.
 * UTF-16 is handled as `proven_u16` code units in native byte order; byte order only matters
 * once text becomes bytes, which is `stream.h`'s job.
 */

/**
 * @brief How text is laid out as bytes, for the readers and writers in `stream.h`.
 */
typedef enum {
    PROVEN_TEXT_UTF8 = 0,  /**< UTF-8. The default everywhere. */
    PROVEN_TEXT_UTF16LE,   /**< UTF-16, little-endian code units (what Windows writes). */
    PROVEN_TEXT_UTF16BE,   /**< UTF-16, big-endian code units. */
    PROVEN_TEXT_AUTO,      /**< Readers only: decided by a leading byte order mark, which is
                                consumed; UTF-8 when there is none. Writers refuse it. */
} proven_text_encoding_t;

/**
 * @brief How far a partial conversion got, and why it stopped.
 *
 * `err` is one of:
 * - PROVEN_OK: all of the input was converted.
 * - PROVEN_ERR_OUT_OF_BOUNDS: the next character did not fit in what was left of the output.
 * - PROVEN_ERR_NEED_MORE: the input ends part-way through a character that could still be
 *   valid. `consumed` stops before it; keep those units and retry with more input.
 * - PROVEN_ERR_INVALID_ENCODING: the input at `consumed` is malformed.
 * - PROVEN_ERR_INVALID_ARG: a null pointer with a non-zero size.
 *
 * `consumed` counts input units (bytes for UTF-8, code units for UTF-16) and `written` output
 * units. Both only ever cover whole characters: a character is converted completely or not at
 * all, so a stop never leaves half a surrogate pair or half a UTF-8 sequence in the output.
 */
typedef struct {
    proven_err_t  err;
    proven_size_t consumed;
    proven_size_t written;
} proven_utf_step_t;

// -------------------------------------------------------------
// Measuring
// -------------------------------------------------------------

/**
 * @brief One step of decoding: a character, or what is in the way of one.
 */
typedef struct {
    proven_err_t  err;  /**< OK, INVALID_ENCODING, NEED_MORE, EOF, OUT_OF_BOUNDS or INVALID_ARG */
    proven_u32    cp;   /**< The Unicode scalar value when err is OK; 0 otherwise */
    proven_size_t len;  /**< Bytes this step covers - see proven_utf8_decode_next */
} proven_utf8_char_t;

/**
 * @brief Decode the one character that starts at byte `pos` of `s`.
 *
 * The step for code that walks text a character at a time - a segmenter, a shaper, a lexer -
 * with the same strict rules as every other function here. `len` always says how far to move:
 *
 * - **PROVEN_OK:** `cp` is the character and `len` its byte length, 1 to 4.
 * - **PROVEN_ERR_INVALID_ENCODING:** the bytes at `pos` are not a character and no later byte
 *   can make them one. `len` (at least 1) is the *maximal subpart* - the longest prefix that
 *   was still a valid start - so stepping by it resynchronises exactly where Unicode says to.
 *   Nothing is substituted: a caller that displays text puts one U+FFFD per such step, which is
 *   Unicode's recommended practice (chapter 3, "U+FFFD substitution of maximal subparts").
 * - **PROVEN_ERR_NEED_MORE:** the view ends inside a character that could still be valid.
 *   `len` is the bytes left. Text read in pieces keeps them for the next piece; a complete text
 *   treats them as malformed.
 * - **PROVEN_ERR_EOF:** `pos` is the end of `s`; `len` is 0.
 * - **PROVEN_ERR_OUT_OF_BOUNDS:** `pos` is past the end; **PROVEN_ERR_INVALID_ARG:** `s` has a
 *   NULL pointer with a nonzero size. `len` is 0 for both.
 *
 * So `for (pos = 0; pos < s.size; pos += c.len) { c = proven_utf8_decode_next(s, pos); ... }`
 * always advances, and visits every byte exactly once. Pure computation, freestanding-available.
 */
[[nodiscard]]
proven_utf8_char_t proven_utf8_decode_next(proven_u8str_view_t s, proven_size_t pos);

/**
 * @brief The number of UTF-16 code units `src` converts to.
 * @return PROVEN_ERR_INVALID_ENCODING for malformed input or input that ends mid-character.
 */
[[nodiscard]]
proven_result_size_t proven_utf8_to_utf16_size(proven_u8str_view_t src);

/**
 * @brief The number of UTF-8 bytes `n` UTF-16 code units convert to.
 * @return PROVEN_ERR_INVALID_ENCODING for an unpaired surrogate, a trailing one included.
 */
[[nodiscard]]
proven_result_size_t proven_utf16_to_utf8_size(const proven_u16 *src, proven_size_t n);

// -------------------------------------------------------------
// Fixed capacity, atomic
// -------------------------------------------------------------

/**
 * @brief Convert all of `src` into `out`, or nothing.
 *
 * The input is validated and measured before a unit is written, so on any failure `out` is
 * untouched and `*written` is 0. No terminator is written.
 *
 * @return PROVEN_ERR_OUT_OF_BOUNDS when the result needs more than `out_cap` units;
 *         PROVEN_ERR_INVALID_ENCODING for malformed or truncated input.
 */
[[nodiscard]]
proven_err_t proven_utf8_to_utf16(proven_u8str_view_t src, proven_u16 *out, proven_size_t out_cap,
                                  proven_size_t *written);

/** @brief The same, from UTF-16 code units into UTF-8 bytes. */
[[nodiscard]]
proven_err_t proven_utf16_to_utf8(const proven_u16 *src, proven_size_t n, proven_byte_t *out,
                                  proven_size_t out_cap, proven_size_t *written);

// -------------------------------------------------------------
// Partial: for text that arrives in pieces
// -------------------------------------------------------------

/**
 * @brief Convert as much of `src` as fits in `out`, stopping at a whole character.
 * @see proven_utf_step_t for what each stop means.
 */
[[nodiscard]]
proven_utf_step_t proven_utf8_to_utf16_partial(proven_u8str_view_t src, proven_u16 *out, proven_size_t out_cap);

/** @brief The same, from UTF-16 code units into UTF-8 bytes. A high surrogate as the last
 *         unit is PROVEN_ERR_NEED_MORE: its low half may be in the next piece. */
[[nodiscard]]
proven_utf_step_t proven_utf16_to_utf8_partial(const proven_u16 *src, proven_size_t n,
                                               proven_byte_t *out, proven_size_t out_cap);

// -------------------------------------------------------------
// Growable, atomic: append to an owned string
// -------------------------------------------------------------

/**
 * @brief Append the UTF-8 form of `n` UTF-16 code units to `dst`, growing it.
 *
 * All or nothing: on failure `dst` keeps its old length and terminator. `src` must not point
 * into `dst`'s own storage (PROVEN_ERR_INVALID_ARG) - growing it would move the input.
 */
[[nodiscard]]
proven_err_t proven_utf16_append_to_u8str(proven_allocator_t alloc, proven_u8str_t *dst,
                                          const proven_u16 *src, proven_size_t n);

#ifndef PROVEN_NO_U16STR
/**
 * @brief Append the UTF-16 form of `src` to `dst`, growing it. All or nothing, as above.
 */
[[nodiscard]]
proven_err_t proven_utf8_append_to_u16str(proven_allocator_t alloc, proven_u16str_t *dst,
                                          proven_u8str_view_t src);
#endif

#endif /* PROVEN_UTF_H */
