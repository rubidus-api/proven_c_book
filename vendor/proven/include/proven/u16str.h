#ifndef PROVEN_U16STR_H
#define PROVEN_U16STR_H

#ifndef PROVEN_NO_U16STR

#include "proven/types.h"
#include "proven/buffer.h"
#include "proven/allocator.h"
#include "proven/u8str.h"   /* PROVEN_INDEX_NOT_FOUND */

/**
 * @file u16str.h
 * @brief String wrappers enforcing Unsigned 16-bit (encoding-agnostic) logic.
 * 
 * Length is calculated in units of u16, and the null terminator is a u16 0.
 * Encoding assumptions (like UTF-16) are deferred to higher layers.
 */

/**
 * @brief Zero-copy view into a U16 string.
 */
typedef struct {
    const proven_u16 *ptr;
    proven_size_t     size; /**< Number of u16 units, not bytes */
} proven_u16str_view_t;

/**
 * @brief Managed U16 string structure.
 *
 * @warning **The string does not remember its allocator, and nothing checks that you pass the
 *          same one.** create, reserve, append_grow and destroy must all be given the
 *          allocator the string was created with. Passing another - destroying an arena string
 *          through the heap, growing a heap string through an arena - is not an error this
 *          library can report: it corrupts the allocator's state, and the damage surfaces
 *          later, somewhere else. Keep a string and its allocator together in your own code.
 *          (Storing the allocator in every string was rejected - it doubles the struct and has
 *          no meaning for a borrowed string; see B-023.)
 */
typedef struct {
    proven_buf_t internal; /**< Capacity and length are tracked in bytes */
} proven_u16str_t;

/**
 * @brief Result wrapper for u16 string creation.
 */
typedef struct {
    proven_err_t    err;
    proven_u16str_t value;
} proven_result_u16str_t;

/**
 * @brief A borrowed u16 view, or the error that stopped it (the u16 line readers return this).
 */
typedef struct {
    proven_err_t         err;
    proven_u16str_view_t val;
} proven_result_u16str_view_t;

#define PROVEN_U16_LIT(s) ((proven_u16str_view_t){ u##s, (sizeof(u##s) / sizeof((u##s)[0])) - 1 })

[[nodiscard]] proven_result_u16str_t proven_u16str_create(proven_allocator_t alloc, proven_size_t unit_limit);
[[nodiscard]] proven_result_u16str_t proven_u16str_create_from_view(proven_allocator_t alloc, proven_u16str_view_t view);
/** @brief Free an owned string. `alloc` MUST be the allocator it was created with - unchecked. */
void proven_u16str_destroy(proven_allocator_t alloc, proven_u16str_t *str);

/**
 * @brief Appends data to a U16 string. 
 * CATEGORY: Atomic Fixed-Capacity
 * 
 * If the data fits entirely within the current capacity, it appends and returns PROVEN_OK.
 * If not, it returns PROVEN_ERR_OUT_OF_BOUNDS without modifying the original string.
 */
[[nodiscard]] proven_err_t proven_u16str_append(proven_u16str_t *str, proven_u16str_view_t data);

/**
 * @brief Appends data as much as possible.
 * CATEGORY: Best-Effort/Truncating
 */
[[nodiscard]] proven_result_size_t proven_u16str_append_partial(proven_u16str_t *str, proven_u16str_view_t data);

/**
 * @brief Appends a view. Potentially grows the buffer using the provided allocator.
 * CATEGORY: Atomic Growable
 */
[[nodiscard]] proven_err_t proven_u16str_append_grow(proven_allocator_t alloc, proven_u16str_t *str, proven_u16str_view_t data);

[[nodiscard]]
static inline const proven_u16* proven_u16str_as_ptr(const proven_u16str_t *str) {
    return (const proven_u16*)str->internal.ptr;
}

[[nodiscard]]
static inline proven_size_t proven_u16str_len(const proven_u16str_t *str) {
    return str->internal.len / sizeof(proven_u16);
}

/** @brief Zero-cost downgrade of a string to a view; invalidated by anything that may reallocate. */
[[nodiscard]]
static inline proven_u16str_view_t proven_u16str_as_view(const proven_u16str_t *str) {
    if (!str) return (proven_u16str_view_t){ (const proven_u16 *)0, 0 };
    return (proven_u16str_view_t){ (const proven_u16 *)str->internal.ptr, str->internal.len / sizeof(proven_u16) };
}

// -------------------------------------------------------------
// The view vocabulary (RFC-0044, since v0.112.0): u8str.h's, in 16-bit units.
//
// For code that holds NUL-terminated 16-bit text - Win32's wide strings - and would otherwise
// reach for wcslen, wcscmp, wcsstr, wcstok, wcstoul or wcsncpy. Every function below is a pure
// function of its arguments: no allocation, no hidden state, no platform call (but, like the
// rest of this header, left out of the freestanding profile by PROVEN_NO_U16STR). The rules
// are u8str.h's: an ILL-FORMED view - ptr == NULL with size > 0 - is treated as empty; every
// empty result is {NULL, 0}; positions and sizes count units, not bytes; a search that finds
// nothing returns PROVEN_INDEX_NOT_FOUND. Still encoding-agnostic: a unit is compared, copied
// and cut as a number, so a surrogate pair is two units to every function here (convert with
// utf.h when the text must be well-formed UTF-16).
//
// On Windows `wchar_t` is a 16-bit unsigned integer, the same type as proven_u16 in C, so a
// `const wchar_t *` from a Win32 call passes to proven_u16str_view_from_cstr without a cast.
// -------------------------------------------------------------

/** @brief The number of units before the first 0 unit. NULL is 0. */
[[nodiscard]] proven_size_t proven_u16_cstr_len(const proven_u16 *s);

/** @brief A view of a NUL-terminated 16-bit string, without its terminator. NULL gives {NULL, 0}. */
[[nodiscard]] static inline proven_u16str_view_t proven_u16str_view_from_cstr(const proven_u16 *s) {
    if (!s) return (proven_u16str_view_t){ (const proven_u16 *)0, 0 };
    return (proven_u16str_view_t){ s, proven_u16_cstr_len(s) };
}

/** @brief Same length and the same units: 1 or 0. */
[[nodiscard]] int proven_u16str_view_eq(proven_u16str_view_t a, proven_u16str_view_t b);
/**
 * @brief Order two views by unit value, unsigned; a proper prefix sorts first (wcscmp's order
 *        where wchar_t is 16-bit). A negative value, zero or a positive value: test the sign.
 */
[[nodiscard]] int proven_u16str_view_cmp(proven_u16str_view_t a, proven_u16str_view_t b);
[[nodiscard]] int proven_u16str_view_starts_with(proven_u16str_view_t str, proven_u16str_view_t prefix);
[[nodiscard]] int proven_u16str_view_ends_with(proven_u16str_view_t str, proven_u16str_view_t suffix);

/**
 * @brief The first position at or after `start` where `needle` occurs, or PROVEN_INDEX_NOT_FOUND.
 *        An empty needle matches at `start`; a `start` past the end finds nothing.
 * @note Cost: a scan for the needle's first unit, then a comparison: O(n*m) at worst, which is
 *       wcsstr's own bound in the common C libraries.
 */
[[nodiscard]] proven_size_t proven_u16str_view_find(proven_u16str_view_t haystack, proven_size_t start, proven_u16str_view_t needle);
/** @brief The start of the last occurrence of `needle`, or PROVEN_INDEX_NOT_FOUND. An empty needle gives haystack.size. */
[[nodiscard]] proven_size_t proven_u16str_view_find_last(proven_u16str_view_t haystack, proven_u16str_view_t needle);
/** @brief The first position at or after `start` holding `unit` (wcschr), or PROVEN_INDEX_NOT_FOUND. */
[[nodiscard]] proven_size_t proven_u16str_view_find_unit(proven_u16str_view_t haystack, proven_size_t start, proven_u16 unit);
/** @brief The last position holding `unit` (wcsrchr), or PROVEN_INDEX_NOT_FOUND. */
[[nodiscard]] proven_size_t proven_u16str_view_find_last_unit(proven_u16str_view_t haystack, proven_u16 unit);
[[nodiscard]] bool proven_u16str_view_contains(proven_u16str_view_t haystack, proven_u16str_view_t needle);
/** @brief Up to `len` units from `index`, clamped to the view; {NULL, 0} when nothing is left. */
[[nodiscard]] proven_u16str_view_t proven_u16str_view_slice(proven_u16str_view_t str, proven_size_t index, proven_size_t len);

/** @brief Drop leading and trailing whitespace: exactly the six ASCII units ' ', '\t', '\n', '\v', '\f', '\r'. */
[[nodiscard]] proven_u16str_view_t proven_u16str_view_trim(proven_u16str_view_t s);
[[nodiscard]] proven_u16str_view_t proven_u16str_view_trim_start(proven_u16str_view_t s);
[[nodiscard]] proven_u16str_view_t proven_u16str_view_trim_end(proven_u16str_view_t s);
/** @brief `s` without `prefix` if it starts with it, otherwise `s` unchanged. */
[[nodiscard]] proven_u16str_view_t proven_u16str_view_remove_prefix(proven_u16str_view_t s, proven_u16str_view_t prefix);
/** @brief `s` without `suffix` if it ends with it, otherwise `s` unchanged. */
[[nodiscard]] proven_u16str_view_t proven_u16str_view_remove_suffix(proven_u16str_view_t s, proven_u16str_view_t suffix);

/**
 * @brief One unit with an ASCII letter made lower case: 'A' to 'Z' become 'a' to 'z', every
 *        other unit is returned as it is. No locale and no Unicode case mapping.
 */
[[nodiscard]] static inline proven_u16 proven_u16_ascii_lower(proven_u16 c) {
    return c >= 'A' && c <= 'Z' ? (proven_u16)(c + ('a' - 'A')) : c;
}
/**
 * @brief eq, cmp, starts_with and ends_with with ASCII letters compared without case, by
 *        proven_u16_ascii_lower: what an ordinal case-insensitive comparison of file names,
 *        registry keys or switches needs when they are ASCII. Every unit from 0x80 matches only
 *        itself. cmp orders the lowered units.
 */
[[nodiscard]] int proven_u16str_view_eq_ascii_ci(proven_u16str_view_t a, proven_u16str_view_t b);
[[nodiscard]] int proven_u16str_view_cmp_ascii_ci(proven_u16str_view_t a, proven_u16str_view_t b);
[[nodiscard]] int proven_u16str_view_starts_with_ascii_ci(proven_u16str_view_t str, proven_u16str_view_t prefix);
[[nodiscard]] int proven_u16str_view_ends_with_ascii_ci(proven_u16str_view_t str, proven_u16str_view_t suffix);

/** @brief An iterator over the fields of a view split on a separator. Copyable; the fields are its own. */
typedef struct {
    proven_u16str_view_t rest;
    proven_u16str_view_t sep;
    bool                 done;
} proven_u16str_view_split_t;

/**
 * @brief Begin splitting `src` on the separator `sep`: u8str's contract - n separators yield
 *        n + 1 fields, empty fields kept, "" is one empty field, an empty `sep` one field.
 */
[[nodiscard]] proven_u16str_view_split_t proven_u16str_view_split(proven_u16str_view_t src, proven_u16str_view_t sep);
/** @brief The next field into `*out` and true, or false when there are none. */
[[nodiscard]] bool proven_u16str_view_split_next(proven_u16str_view_split_t *it, proven_u16str_view_t *out);

/** @brief An iterator over the tokens of a view: wcstok without its hidden state. Copyable; the fields are its own. */
typedef struct {
    proven_u16str_view_t rest;
    proven_u16str_view_t delims;
} proven_u16str_view_tokens_t;

/**
 * @brief Begin reading the tokens of `src` between units of the set `delims`: any one unit of
 *        `delims` separates, a run of them is one separation, and no token is empty - so
 *        ";;a;b;" is "a" and "b". An empty `delims` makes the whole of a non-empty `src` one
 *        token. Unlike wcstok, `src` is not written to and two iterations can run at once.
 */
[[nodiscard]] proven_u16str_view_tokens_t proven_u16str_view_tokens(proven_u16str_view_t src, proven_u16str_view_t delims);
/** @brief The next token into `*out` and true, or false when there are none. */
[[nodiscard]] bool proven_u16str_view_tokens_next(proven_u16str_view_tokens_t *it, proven_u16str_view_t *out);

/**
 * @brief An integer from 16-bit text, in base 10 or 16.
 *
 * The digits are the ASCII ones ('0'-'9', and 'a'-'f', 'A'-'F' in base 16). In base 16 a "0x"
 * or "0X" in front is taken only when a hex digit follows it, as proven_scan_u64_hex takes it.
 * The signed forms take one '+' or '-' in front; the unsigned forms take no sign. Stricter than
 * wcstoul and wcstol on purpose: no whitespace is skipped (trim first), and "-1" is not a
 * large unsigned value but an error.
 *
 * @param used NULL: the whole view must be the number. Otherwise the longest number at the
 *        front is read and the units it took are written here, the rest left to the caller.
 * @return PROVEN_OK and `*out`; PROVEN_ERR_INVALID_ARG when there is no digit, when the view has
 *         more than the number (`used` NULL), for a base other than 10 and 16 or a NULL `out`;
 *         PROVEN_ERR_OVERFLOW when the value does not fit. `*out` is written only on PROVEN_OK.
 */
[[nodiscard]] proven_err_t proven_u16str_view_parse_u64(proven_u16str_view_t s, unsigned base, proven_size_t *used, proven_u64 *out);
[[nodiscard]] proven_err_t proven_u16str_view_parse_i64(proven_u16str_view_t s, unsigned base, proven_size_t *used, proven_i64 *out);
[[nodiscard]] proven_err_t proven_u16str_view_parse_u32(proven_u16str_view_t s, unsigned base, proven_size_t *used, proven_u32 *out);
[[nodiscard]] proven_err_t proven_u16str_view_parse_i32(proven_u16str_view_t s, unsigned base, proven_size_t *used, proven_i32 *out);

/**
 * @brief Copy a view into a caller's buffer of `cap` units and end it with a 0 unit: wcsncpy
 *        made safe.
 * CATEGORY: Best-Effort/Truncating, into plain memory
 *
 * At most `cap - 1` units are copied, and when `cap > 0` the copy always ends with a 0 unit.
 * `value` is the number of units copied (without the 0). When the view does not fit, the copy
 * is cut at a unit - a surrogate pair may be cut in two - and `err` is PROVEN_ERR_OUT_OF_BOUNDS;
 * `cap == 0` copies nothing and is OUT_OF_BOUNDS. `buf` and the view must not overlap.
 */
[[nodiscard]] proven_result_size_t proven_u16str_view_copy_to(proven_u16 *buf, proven_size_t cap, proven_u16str_view_t view);

#endif /* PROVEN_NO_U16STR */

#endif // PROVEN_U16STR_H
