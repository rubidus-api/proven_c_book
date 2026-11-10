#ifndef PROVEN_URL_H
#define PROVEN_URL_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/u8str.h"

/**
 * @file url.h
 * @brief URLs: taking one apart, percent-coding, and turning a request path into one that is
 *        safe to open.
 *
 * Everything here is text in and text out. Nothing allocates, nothing touches the network or
 * the filesystem, and every result is either a view into the text you passed or bytes written
 * into memory you supplied - so the header is available in freestanding builds.
 *
 * Two things in it are deliberate.
 *
 * **Parsing does not decode.** proven_url_parse returns the components exactly as they were
 * written, percent signs and all. Decoding is a separate step you take on the one component
 * you are about to use, because a URL decoded as a whole cannot be taken apart again: a `%2F`
 * that was data inside a path segment becomes a `/` that separates segments, and a `%3F`
 * becomes the start of a query.
 *
 * **A path from the network is hostile until proven otherwise.** proven_url_path_resolve is
 * the one function to pass a request path through before it goes near a filesystem. It decodes
 * once, refuses what has no business in a file name, and resolves `.` and `..` - reporting, as
 * an error, a path that would climb above its root.
 *
 * This is RFC 3986 for URLs with an authority (`scheme://host...`). It is not the WHATWG URL
 * algorithm that browsers implement: nothing is repaired, guessed or normalised behind your
 * back, and text a browser would fix up is refused here.
 *
 * Not part of the socket layer: `PROVEN_NO_NET` does not remove this header.
 */

// -----------------------------------------------------------------------------
// Taking a URL apart
// -----------------------------------------------------------------------------

/**
 * @brief The components of a URL, each a view into the text that was parsed.
 *
 * `scheme://userinfo@host:port/path?query#fragment`. Every view is the component as written,
 * still percent-encoded, without its delimiter. The `has_` flags tell an absent component from
 * an empty one: `http://h/?` has a query that is empty, `http://h/` has none.
 */
typedef struct {
    proven_u8str_view_t scheme;     /**< `https` - letters as written; compare without case */
    proven_u8str_view_t userinfo;   /**< `user:secret`, when present */
    proven_u8str_view_t host;       /**< a name, an IPv4 literal, or an IPv6 literal WITHOUT its brackets */
    proven_u8str_view_t path;       /**< `/a/b`; may be empty */
    proven_u8str_view_t query;      /**< after `?`, without it */
    proven_u8str_view_t fragment;   /**< after `#`, without it */
    proven_u16 port;                /**< the port that was written; 0 when none was */
    bool has_userinfo;
    bool has_port;
    bool has_query;
    bool has_fragment;
    bool host_is_ipv6;              /**< the host was written in brackets */
} proven_url_t;

/**
 * @brief Split a URL into its components.
 *
 * Accepts `scheme://authority[path][?query][#fragment]`. The text must be ASCII with no
 * spaces and no control characters, every `%` must be followed by two hex digits, and the port
 * must be a number up to 65535. An IPv6 host is written in brackets; its contents are checked
 * for shape only (hex digits, colons, dots), not parsed.
 *
 * @return PROVEN_ERR_INVALID_FORMAT when `text` is not such a URL - a relative reference, a
 *         `mailto:`-style URL with no authority, or malformed text. `*out` is then untouched.
 */
[[nodiscard]]
proven_err_t proven_url_parse(proven_u8str_view_t text, proven_url_t *out);

/** @brief The port a scheme uses when a URL names none: 80 for `http` and `ws`, 443 for
 *         `https` and `wss`, 21 for `ftp`; 0 for a scheme this library does not know. */
[[nodiscard]]
proven_u16 proven_url_default_port(proven_u8str_view_t scheme);

/** @brief The port to connect to: the one written, or the scheme's default; 0 when neither. */
[[nodiscard]]
proven_u16 proven_url_effective_port(const proven_url_t *url);

/** @brief Whether the URL's scheme is `scheme`, ignoring ASCII case (`HTTP` is `http`). */
[[nodiscard]]
bool proven_url_scheme_is(const proven_url_t *url, proven_u8str_view_t scheme);

/**
 * @brief Split the request target of an HTTP request line into path and query.
 *
 * Takes the three forms a server meets: origin-form (`/a/b?x=1`), absolute-form
 * (`http://host/a/b?x=1`, which every server must accept), and `*` (for `OPTIONS *`), which
 * comes back as the path `*`. Nothing is decoded.
 *
 * @return PROVEN_ERR_INVALID_FORMAT for anything else, authority-form (`host:443`) included -
 *         that is the target of CONNECT, and has no path.
 */
[[nodiscard]]
proven_err_t proven_url_split_target(proven_u8str_view_t target, proven_u8str_view_t *path,
                                     proven_u8str_view_t *query, bool *has_query);

/**
 * @brief Resolve a reference against a base URL (RFC 3986 section 5.2), writing the result as
 *        text: what a `Location` header or a link means, given where it was found.
 *
 * `reference` may be an absolute URL (returned as it is), `//host/path` (the base's scheme is
 * kept), `/path` (the base's scheme and authority), a relative path (`../a`, `b?x`), a bare
 * query (`?x=1`) or a bare fragment. Dot segments in the result's path are removed; a `..`
 * that would climb above the root stops at the root, as the RFC specifies. Nothing is decoded.
 * The base's fragment is never carried over.
 *
 * `out` needs at most `base.size + reference.size + 1` bytes.
 *
 * @return PROVEN_ERR_INVALID_FORMAT when `base` is not an absolute URL, when the result is not
 *         one, or when `reference` holds a space, a control character or a broken `%` escape.
 *         PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small.
 */
[[nodiscard]]
proven_err_t proven_url_resolve(proven_u8str_view_t base, proven_u8str_view_t reference,
                                proven_mem_mut_t out, proven_size_t *written);

// -----------------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------------

/** @brief Where a walk over `name=value&name=value` has got to. Caller-owned; start it with
 *         proven_url_query_iter. */
typedef struct {
    proven_u8str_view_t rest;
} proven_url_query_iter_t;

/** @brief Begin walking a query string (without its `?`). */
[[nodiscard]]
proven_url_query_iter_t proven_url_query_iter(proven_u8str_view_t query);

/**
 * @brief The next `name=value` pair, as two views into the query, still encoded.
 *
 * Pairs are separated by `&`; the name ends at the first `=`. A pair with no `=` has an empty
 * value; empty pairs (`a=1&&b=2`) are skipped. Decode a name or value with
 * proven_url_form_decode.
 *
 * @return false when there are no more pairs.
 */
[[nodiscard]]
bool proven_url_query_next(proven_url_query_iter_t *it, proven_u8str_view_t *name, proven_u8str_view_t *value);

// -----------------------------------------------------------------------------
// Percent-coding
// -----------------------------------------------------------------------------

/**
 * @brief Turn `%XX` back into bytes.
 *
 * The output is never longer than the input, so `out` of `in.size` bytes is always enough, and
 * `out.ptr` may be `in.ptr` itself: decoding in place is allowed. `+` is left as `+`.
 *
 * The result is BYTES. It may contain a NUL, a slash, or bytes that are not UTF-8 - whatever
 * the sender chose to encode. Validate it for what you are about to do with it; for a path,
 * use proven_url_path_resolve instead.
 *
 * @return PROVEN_ERR_INVALID_FORMAT for a `%` not followed by two hex digits (nothing useful is
 *         in `out`); PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small.
 */
[[nodiscard]]
proven_err_t proven_url_percent_decode(proven_u8str_view_t in, proven_mem_mut_t out, proven_size_t *written);

/** @brief proven_url_percent_decode for form data (`application/x-www-form-urlencoded`), where
 *         `+` also means a space. Use it for query names and values. */
[[nodiscard]]
proven_err_t proven_url_form_decode(proven_u8str_view_t in, proven_mem_mut_t out, proven_size_t *written);

/** @brief Bytes that are always enough to encode `n` input bytes; SIZE_MAX when that cannot be
 *         represented. */
[[nodiscard]]
proven_size_t proven_url_encoded_size_max(proven_size_t n);

/**
 * @brief Encode bytes for use as ONE component: a path segment, a query value, a user name.
 *
 * Everything except letters, digits and `-` `.` `_` `~` becomes `%XX` (uppercase hex), so the
 * result contains no character that could end the component it is placed in.
 *
 * @return PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small; PROVEN_ERR_OVERFLOW when the
 *         output size cannot be represented.
 */
[[nodiscard]]
proven_err_t proven_url_encode_component(proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written);

/** @brief Encode a whole path: as proven_url_encode_component, but `/` is kept, so the
 *         segments stay separate. Do not use it on a single segment that may contain a `/`. */
[[nodiscard]]
proven_err_t proven_url_encode_path(proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written);

/** @brief Encode a form name or value: as proven_url_encode_component, but a space becomes
 *         `+`, the spelling form data uses. */
[[nodiscard]]
proven_err_t proven_url_form_encode(proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written);

/**
 * @brief Append one `name=value` pair to form data being built in `out`, encoding both.
 *
 * Appends at `*len` and advances it, writing `&` first when `*len` is not 0 - so a run of calls
 * starting from an empty buffer builds `a=1&b=two+words`, ready to be a query string or an
 * `application/x-www-form-urlencoded` body. On failure `*len` is unchanged.
 *
 * @return PROVEN_ERR_OUT_OF_BOUNDS when the pair does not fit.
 */
[[nodiscard]]
proven_err_t proven_url_form_append(proven_mem_mut_t out, proven_size_t *len,
                                    proven_mem_view_t name, proven_mem_view_t value);

// -----------------------------------------------------------------------------
// A path that is safe to use
// -----------------------------------------------------------------------------

/**
 * @brief Turn the path of a request into a clean, decoded path that stays inside its root.
 *
 * `raw_path` is the path as it came off the wire: it starts with `/` and is still
 * percent-encoded. On success `out` holds a path that starts with `/`, is decoded, has no `.`
 * or `..` segments and no empty segments, and keeps a trailing `/` if the request had one.
 * `out` needs at most `raw_path.size` bytes.
 *
 * It decodes ONCE, then resolves the dot segments of the decoded text - in that order, so that
 * `%2e%2e` is the `..` it decodes to, and so that `%252e%252e` is the harmless file name
 * `%2e%2e` and nothing more.
 *
 * Refused, as PROVEN_ERR_INVALID_FORMAT:
 *   - a path that does not start with `/`, or a malformed `%` escape;
 *   - an encoded `/` (`%2F`): it would turn one segment into two;
 *   - a backslash, written or encoded: on Windows it is a separator;
 *   - a NUL or any other control character, written or encoded;
 *   - text that is not valid UTF-8 once decoded - which is how an overlong `%c0%ae` ("an
 *     encoded dot") is caught.
 *
 * @return PROVEN_ERR_PERMISSION when a `..` would climb above the root: the path is well formed
 *         and asks for something it may not have (an attack or a mistake; answer 400 or 403).
 *         PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small.
 *
 * @warning This stops traversal by path syntax. It does not know your filesystem: a symbolic
 *          link inside the root can still point outside it, and Windows has file names with a
 *          meaning of their own (`CON`, a trailing dot, `name:stream`). Check those where the
 *          file is opened.
 */
[[nodiscard]]
proven_err_t proven_url_path_resolve(proven_u8str_view_t raw_path, proven_mem_mut_t out, proven_size_t *written);

#endif /* PROVEN_URL_H */
