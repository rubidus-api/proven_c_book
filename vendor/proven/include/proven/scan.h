#ifndef PROVEN_SCAN_H
#define PROVEN_SCAN_H

#include "proven/types.h"
#include "proven/u8str.h"
#include "proven/error.h"

typedef struct {
    proven_err_t err;
    proven_i64 val;
} proven_result_i64_t;

typedef struct {
    proven_err_t err;
    proven_u64 val;
} proven_result_u64_t;

typedef struct {
    proven_err_t err;
    double val;
} proven_result_f64_t;

typedef struct {
    proven_err_t err;
    proven_u8str_view_t val;
} proven_result_u8str_view_t;

/**
 * @struct proven_scan_t
 * @brief Stateful scanner for parsing data from a string view.
 */
typedef struct {
    proven_u8str_view_t view;
    proven_size_t cursor;

    /**
     * @brief The last failure happened because the input RAN OUT, not because it was wrong.
     *
     * For a complete view those are the same fact - there is no more input, so a token cut
     * off at the end really is malformed. For a STREAM they are opposite facts, and the
     * scanner had no way to tell them apart: a pipe that delivered "-" and then, 150 ms
     * later, "12" made proven_scan_i64 report PROVEN_ERR_INVALID_ARG, and the buffered
     * scanner on top of it had nothing to distinguish "this input is garbage" from "ask me
     * again when more has arrived". Set on failure, cleared at the start of every scan.
     */
    bool needs_more;
} proven_scan_t;

/**
 * @brief Initialize a scanner with a string view.
 * Normalizes invalid views (size > 0 && !ptr) to empty views.
 */
static inline proven_scan_t proven_scan_init(proven_u8str_view_t view) {
    if (view.size > 0 && !view.ptr) {
        view = (proven_u8str_view_t){ .ptr = (const proven_byte_t*)0, .size = 0 };
    }
    return (proven_scan_t){ .view = view, .cursor = 0 };
}

/**
 * @brief Skip leading whitespace.
 */
void proven_scan_skip_whitespace(proven_scan_t *scan);

/**
 * @brief Extract a 64-bit signed integer.
 */
[[nodiscard]] proven_result_i64_t proven_scan_i64(proven_scan_t *scan);

/**
 * @brief Extract a 64-bit unsigned integer.
 */
[[nodiscard]] proven_result_u64_t proven_scan_u64(proven_scan_t *scan);

/**
 * @brief Extract a 64-bit floating point number.
 * Failure restores the cursor to its original position.
 *
 * Decimal-to-double conversion routes through the shared ASCII float backend.
 * Finite decimal inputs are rounded to IEEE-754 binary64 with
 * round-to-nearest, ties-to-even behavior. Values below the half-way threshold
 * to the smallest subnormal round to signed zero with the input sign
 * preserved.
 */
[[nodiscard]] proven_result_f64_t proven_scan_f64(proven_scan_t *scan);

/**
 * @brief Extract a string (delimited by whitespace).
 * Returns a view into the original string.
 */
[[nodiscard]] proven_result_u8str_view_t proven_scan_str(proven_scan_t *scan);

/**
 * @brief Skip current input until the target substring is found.
 * If found, cursor is placed at the start of the target.
 * If not found, cursor remains at current position and returns error.
 */
[[nodiscard]] proven_err_t proven_scan_skip_until(proven_scan_t *scan, proven_u8str_view_t target);

/**
 * @brief Skip all characters until a valid number (including its sign) is encountered.
 * Stops at the first digit (0-9) or a sign (+, -) immediately followed by a digit.
 */
void proven_scan_skip_until_number(proven_scan_t *scan);

/**
 * @brief Supported argument types for the structural scanner.
 */
typedef enum {
    PROVEN_SCAN_ARG_TYPE_NONE,
    PROVEN_SCAN_ARG_TYPE_I32,
    PROVEN_SCAN_ARG_TYPE_U32,
    PROVEN_SCAN_ARG_TYPE_I64,
    PROVEN_SCAN_ARG_TYPE_U64,
    PROVEN_SCAN_ARG_TYPE_SHORT,
    PROVEN_SCAN_ARG_TYPE_USHORT,
    PROVEN_SCAN_ARG_TYPE_INT,
    PROVEN_SCAN_ARG_TYPE_UINT,
    PROVEN_SCAN_ARG_TYPE_LONG,
    PROVEN_SCAN_ARG_TYPE_ULONG,
    PROVEN_SCAN_ARG_TYPE_LLONG,
    PROVEN_SCAN_ARG_TYPE_ULLONG,
    PROVEN_SCAN_ARG_TYPE_F64,
    PROVEN_SCAN_ARG_TYPE_STR_VIEW,
} proven_scan_arg_type_t;

/**
 * @brief Container for a single format scan argument.
 */
typedef struct {
    proven_scan_arg_type_t type;
    union {
        proven_i32 *i32;
        proven_u32 *u32;
        proven_i64 *i64;
        proven_u64 *u64;
        short *s;
        unsigned short *us;
        int *i;
        unsigned int *ui;
        long *l;
        unsigned long *ul;
        long long *ll;
        unsigned long long *ull;
        double     *f64;
        proven_u8str_view_t *str_view;
    } ptr;
} proven_scan_arg_t;

/**
 * @brief Simple argument constructors.
 *
 * Note: PROVEN_SCAN_ARG(&x) selects by the variable's underlying native C type.
 * For fixed proven integer destinations, this is still safe because proven_i32
 * and proven_i64 are typedefs of native integer types.
 *
 * The explicit proven_scan_arg_i32/i64/u32/u64 helpers remain available for
 * callers that want to construct proven_scan_arg_t values manually.
 */
static inline proven_scan_arg_t proven_scan_arg_none(void) { return (proven_scan_arg_t){PROVEN_SCAN_ARG_TYPE_NONE, {0}}; }
static inline proven_scan_arg_t proven_scan_arg_i32(proven_i32 *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_I32, .ptr = {.i32 = v}}; }
static inline proven_scan_arg_t proven_scan_arg_u32(proven_u32 *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_U32, .ptr = {.u32 = v}}; }
static inline proven_scan_arg_t proven_scan_arg_i64(proven_i64 *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_I64, .ptr = {.i64 = v}}; }
static inline proven_scan_arg_t proven_scan_arg_u64(proven_u64 *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_U64, .ptr = {.u64 = v}}; }
static inline proven_scan_arg_t proven_scan_arg_short(short *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_SHORT, .ptr = {.s = v}}; }
static inline proven_scan_arg_t proven_scan_arg_ushort(unsigned short *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_USHORT, .ptr = {.us = v}}; }
static inline proven_scan_arg_t proven_scan_arg_int(int *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_INT, .ptr = {.i = v}}; }
static inline proven_scan_arg_t proven_scan_arg_uint(unsigned int *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_UINT, .ptr = {.ui = v}}; }
static inline proven_scan_arg_t proven_scan_arg_long(long *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_LONG, .ptr = {.l = v}}; }
static inline proven_scan_arg_t proven_scan_arg_ulong(unsigned long *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_ULONG, .ptr = {.ul = v}}; }
static inline proven_scan_arg_t proven_scan_arg_llong(long long *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_LLONG, .ptr = {.ll = v}}; }
static inline proven_scan_arg_t proven_scan_arg_ullong(unsigned long long *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_ULLONG, .ptr = {.ull = v}}; }
static inline proven_scan_arg_t proven_scan_arg_f64(double *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_F64, .ptr = {.f64 = v}}; }
static inline proven_scan_arg_t proven_scan_arg_str_view(proven_u8str_view_t *v) { return (proven_scan_arg_t){.type = PROVEN_SCAN_ARG_TYPE_STR_VIEW, .ptr = {.str_view = v}}; }

/**
 * @brief Identity function for arguments that are already proven_scan_arg_t.
 */
static inline proven_scan_arg_t proven_scan_arg_identity(proven_scan_arg_t v) { return v; }

/**
 * @brief Type-safe argument selection using C11 _Generic for pointers.
 * Mapping is done to native C types to avoid selection conflicts with fixed-width typedefs (e.g. long vs int64_t).
 *
 * Native integer destinations are range-checked through the scanner's internal
 * proven_i64/proven_u64 parser. On platforms where a native integer type is wider
 * than 64 bits, scan support is limited to the proven_i64/proven_u64 range.
 */
#define PROVEN_SCAN_ARG(x) _Generic((x), \
    short*:              proven_scan_arg_short, \
    unsigned short*:     proven_scan_arg_ushort, \
    int*:                proven_scan_arg_int, \
    unsigned int*:       proven_scan_arg_uint, \
    long*:               proven_scan_arg_long, \
    unsigned long*:      proven_scan_arg_ulong, \
    long long*:          proven_scan_arg_llong, \
    unsigned long long*: proven_scan_arg_ullong, \
    double*:             proven_scan_arg_f64, \
    proven_u8str_view_t*: proven_scan_arg_str_view, \
    proven_scan_arg_t:    proven_scan_arg_identity \
)(x)

/**
 * @brief Explicit aliases for platform-native long destinations.
 *
 * PROVEN_SCAN_ARG(&x) already supports long* and unsigned long*.
 * These macros are provided for callers who want to be explicit at the call site.
 *
 * These helpers store through long* / unsigned long* directly and do not cast
 * to proven_i32*, proven_i64*, proven_u32*, or proven_u64*.
 */
#define PROVEN_SCAN_ARG_LONG(ptr)  proven_scan_arg_long(ptr)
#define PROVEN_SCAN_ARG_ULONG(ptr) proven_scan_arg_ulong(ptr)

/**
 * @brief Modern structural scanner.
 *
 * The args array is expected to include a leading proven_scan_arg_none()
 * sentinel at index 0. User code should normally call the public
 * scanning macros instead of this function.
 *
 * Expects exactly as many {} in the format string as provided arguments.
 * Double destinations use the same floating-point conversion limits as
 * proven_scan_f64.
 *
 * Note: This function may advance the cursor and write destination values
 * before returning an error (e.g. if a literal mismatch occurs after a placeholder).
 * If transactional parsing is required, save scan.cursor and destination 
 * values before calling and restore them on failure.
 */
[[nodiscard]]
proven_err_t proven_scan_fmt_internal(proven_scan_t *scan, const char *fmt, const proven_scan_arg_t *args, proven_size_t args_count);

static inline proven_err_t proven_scan_fmt_internal_view(proven_u8str_view_t view, const char *fmt, const proven_scan_arg_t *args, proven_size_t count) {
    proven_scan_t scan = proven_scan_init(view);
    return proven_scan_fmt_internal(&scan, fmt, args, count);
}

/**
 * @brief Variadic macro to scan from a given proven_scan_t cursor element.
 * Usage: proven_scan_fmt_cursor(&scan, "Hello {}", PROVEN_SCAN_ARG(&num));
 */
#define proven_scan_fmt_cursor(scan_ptr, fmt, ...) \
    proven_scan_fmt_internal(scan_ptr, fmt, \
        (proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }, \
        (sizeof((proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_scan_arg_t)))

/**
 * @brief Variadic macro to scan strings into strictly-typed out-pointers from a view.
 * Usage: proven_scan_fmt(view, "Hello {}", PROVEN_SCAN_ARG(&num));
 */
#define proven_scan_fmt(view, fmt, ...) \
    proven_scan_fmt_internal_view(view, fmt, \
        (proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }, \
        (sizeof((proven_scan_arg_t[]){ proven_scan_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_scan_arg_t)))

#endif // PROVEN_SCAN_H
