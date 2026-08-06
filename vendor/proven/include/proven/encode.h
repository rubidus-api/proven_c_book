#ifndef PROVEN_ENCODE_H
#define PROVEN_ENCODE_H

/**
 * @file encode.h
 * @brief Bytes to text and back — hex and Base64 — by use case.
 *
 * Once you can hash a thing and draw a random token, you need to write those bytes somewhere
 * that only holds text: a URL, an HTTP header, a log line, a JSON string, a config file. That
 * is what this is. There is no cryptography here and no compression — just the two encodings
 * everything else already agrees on, done without hidden allocation and without the two ways
 * they are usually got wrong.
 *
 * The two ways they are usually got wrong, refused here:
 *
 *   - **A decode that trusts its input.** Text from outside your program is not guaranteed to
 *     be valid hex or Base64. A decoder that reads past the end, or quietly accepts a stray
 *     byte and returns a shorter result than the caller sized for, turns malformed input into
 *     a memory bug or a silent truncation. Every decode here validates first and reports
 *     `PROVEN_ERR_INVALID_ENCODING` rather than guessing.
 *
 *   - **A buffer that is one byte too small.** Encoding grows the data by a known factor and a
 *     caller who sizes the output by eye gets it wrong. The size is a function you call, not a
 *     number you remember: `proven_hex_encoded_size`, `proven_base64_encoded_size`, and their
 *     decode counterparts. Pass a buffer smaller than that and the call refuses with
 *     `PROVEN_ERR_OUT_OF_BOUNDS` — it does not write a truncated prefix.
 *
 * Everything here is pure computation: no allocation, no OS, no libc, and it is available
 * freestanding.
 *
 * ## Which encoding
 *
 * | You want | Use | Why |
 * |---|---|---|
 * | A digest or a few bytes, readable, unambiguous. | hex (`proven_hex_encode`) | Two chars per byte, no padding, case-insensitive to decode. What `sha256sum` and `git` print. |
 * | Bytes in a URL or a filename. | Base64URL (`proven_base64url_encode`) | Uses `-` and `_`, so nothing needs escaping in a URL, and no `=` padding to trip a parser. |
 * | Bytes in an HTTP header, a MIME body, JSON. | Base64 (`proven_base64_encode`) | The standard `+` `/` `=` alphabet everything on that side of the wire expects. |
 */

#include "types.h"
#include "memory.h"

// -------------------------------------------------------------
// Hex
// -------------------------------------------------------------

/**
 * @brief The number of characters `proven_hex_encode` writes for `n` input bytes.
 * @note Two per byte, no NUL. `proven_hex_encode` does not terminate; if you want a C string,
 *       size for this + 1 and place the NUL yourself, or use the encoding into a proven_u8str.
 */
[[nodiscard]]
proven_size_t proven_hex_encoded_size(proven_size_t n);

/**
 * @brief The maximum number of bytes `proven_hex_decode` writes for `n` input characters.
 * @note `n / 2`. An odd `n` is malformed and the decode will refuse it.
 */
[[nodiscard]]
proven_size_t proven_hex_decoded_size(proven_size_t n);

/**
 * @brief Encode `data` as lowercase hex into `out`.
 *
 * @param out       caller-owned. Must be at least `proven_hex_encoded_size(data.size)`.
 * @param written_out receives the number of characters written; may be NULL.
 * @return PROVEN_OK; PROVEN_ERR_OUT_OF_BOUNDS if `out` is too small (nothing is written);
 *         PROVEN_ERR_INVALID_ARG for a NULL out with a nonzero size, or a `{NULL, >0}` view.
 * @note Lowercase, to match sha256sum and git. Decoding accepts either case.
 */
[[nodiscard]]
proven_err_t proven_hex_encode(proven_mem_view_t data, proven_byte_t *out, proven_size_t out_cap,
                               proven_size_t *written_out);

/**
 * @brief Decode hex `text` into `out`.
 *
 * @param out       at least `proven_hex_decoded_size(text.size)` bytes.
 * @return PROVEN_ERR_INVALID_ENCODING if `text` has an odd length or any non-hex character —
 *         nothing partial is committed; PROVEN_ERR_OUT_OF_BOUNDS if `out` is too small.
 * @note Upper and lower case both decode. Whitespace is NOT skipped: a space is invalid, so a
 *       caller who pasted a spaced hex dump learns it here rather than one byte into the result.
 */
[[nodiscard]]
proven_err_t proven_hex_decode(proven_mem_view_t text, proven_byte_t *out, proven_size_t out_cap,
                               proven_size_t *written_out);

// -------------------------------------------------------------
// Base64
// -------------------------------------------------------------

/**
 * @brief The number of characters `proven_base64_encode` / `_base64url_encode` writes for `n`
 *        input bytes.
 * @note `4 * ceil(n / 3)`. The standard form pads to a multiple of 4 with `=`; the URL form
 *       (see the note on `proven_base64url_encode`) does not, so its size can be smaller — this
 *       returns the padded size, which is safe for both.
 */
[[nodiscard]]
proven_size_t proven_base64_encoded_size(proven_size_t n);

/**
 * @brief The maximum number of bytes a Base64 `text` of `n` characters can decode to.
 * @note An upper bound over both the padded and the UNPADDED form: `((n + 3) / 4) * 3`. The
 *       exact count depends on the padding and is reported by the decode. It has to round `n`
 *       up rather than down, or it would under-report the 1-2 bytes an unpadded tail carries -
 *       and a caller sizing a buffer by a floor would fail to decode this library's own
 *       `proven_base64url_encode` output.
 */
[[nodiscard]]
proven_size_t proven_base64_decoded_size(proven_size_t n);

/**
 * @brief Encode `data` as standard Base64 (`+` `/`, `=`-padded) into `out`.
 * @param out at least `proven_base64_encoded_size(data.size)` bytes.
 * @return PROVEN_OK; OUT_OF_BOUNDS if `out` is too small; INVALID_ARG for a `{NULL, >0}` view.
 */
[[nodiscard]]
proven_err_t proven_base64_encode(proven_mem_view_t data, proven_byte_t *out, proven_size_t out_cap,
                                  proven_size_t *written_out);

/**
 * @brief Encode `data` as URL-safe Base64 (`-` `_`, NO padding) into `out`.
 *
 * The form you want for a token in a URL, a cookie, or a filename: nothing in the output needs
 * percent-escaping, and there is no `=` to be mangled by a parser that treats it specially.
 * @note No padding is emitted, so the output is 0-2 characters shorter than
 *       `proven_base64_encoded_size` reports; that function is still a safe upper bound.
 */
[[nodiscard]]
proven_err_t proven_base64url_encode(proven_mem_view_t data, proven_byte_t *out, proven_size_t out_cap,
                                     proven_size_t *written_out);

/**
 * @brief Decode Base64 `text` into `out`.
 *
 * Accepts BOTH alphabets — standard (`+` `/`) and URL-safe (`-` `_`) — and both padded and
 * unpadded input, because a decoder that only accepts what it would itself emit rejects half
 * the Base64 in the world. What it does NOT accept is a stray character or a wrong-length
 * unpadded string: those are PROVEN_ERR_INVALID_ENCODING, not a best-effort guess.
 *
 * @param out at least `proven_base64_decoded_size(text.size)` bytes.
 * @return PROVEN_ERR_INVALID_ENCODING for a bad character, bad padding, or an impossible
 *         length; PROVEN_ERR_OUT_OF_BOUNDS if `out` is too small. Nothing partial is committed.
 * @note Whitespace is not skipped, for the same reason as hex: a decoder that silently drops
 *       it hides the difference between clean input and input that has been mangled.
 */
[[nodiscard]]
proven_err_t proven_base64_decode(proven_mem_view_t text, proven_byte_t *out, proven_size_t out_cap,
                                  proven_size_t *written_out);

#endif /* PROVEN_ENCODE_H */
