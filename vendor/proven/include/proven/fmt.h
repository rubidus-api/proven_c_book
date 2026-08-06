#ifndef PROVEN_FMT_H
#define PROVEN_FMT_H

#include "u8str.h"
#include "time.h"

/**
 * @brief Result of a formatting operation.
 */
typedef struct {
    proven_err_t  err;
    proven_size_t written;  /**< Actual bytes/units written */
    proven_size_t required; /**< Total space required for full output */
} proven_fmt_result_t;

/**
 * @brief Supported argument types for the structural formatter.
 */
typedef enum {
    PROVEN_ARG_NONE,
    PROVEN_ARG_I32,
    PROVEN_ARG_U32,
    PROVEN_ARG_I64,
    PROVEN_ARG_U64,
#ifndef PROVEN_FMT_NO_FLOAT
    PROVEN_ARG_F64,
#endif
    PROVEN_ARG_CSTR,
    PROVEN_ARG_STR_VIEW,
    PROVEN_ARG_DATETIME,
    PROVEN_ARG_PTR,
    PROVEN_ARG_FN,
    PROVEN_ARG_CHAR,
    PROVEN_ARG_BOOL,
    PROVEN_ARG_CUSTOM,
} proven_arg_type_t;

/**
 * @brief Where a custom renderer puts its bytes.
 *
 * Deliberately NOT proven_writer_t: that lives in stream.h, which is hosted-only, and
 * the formatter must keep working on a freestanding target. This is the same idea with
 * no dependencies.
 */
typedef struct {
    void *ctx;
    proven_err_t (*write)(void *ctx, proven_u8str_view_t chunk);
} proven_fmt_sink_t;

/** @brief Emit bytes from inside a custom renderer. */
static inline proven_err_t proven_fmt_put(proven_fmt_sink_t out, proven_u8str_view_t chunk) {
    if (!out.write) return PROVEN_ERR_INVALID_ARG;
    return out.write(out.ctx, chunk);
}

/**
 * @brief Renders one value of a type the library has never heard of.
 *
 * @note It is called TWICE per `{}`: once with a counting sink to measure the output so
 *       that width and alignment can be applied without allocating, and once for real.
 *       It must therefore be deterministic and must not mutate `obj`. If the two passes
 *       disagree the formatter returns PROVEN_ERR_INVALID_ARG rather than emit a
 *       misaligned field.
 */
typedef proven_err_t (*proven_fmt_render_fn)(proven_fmt_sink_t out, const void *obj);

typedef struct {
    const void          *obj;
    proven_fmt_render_fn render;
} proven_arg_custom_t;

/**
 * @brief Container for a single format argument.
 */
typedef struct {
    proven_arg_type_t type;
    union {
        proven_i32 i32;
        proven_u32 u32;
        proven_i64 i64;
        proven_u64 u64;
        double     f64;
        const char  *cstr;
        proven_u8str_view_t str_view;
        proven_datetime_t   datetime;
        const void         *ptr;
        void (*fn)(void);
        char c;
        bool b;
        proven_arg_custom_t custom;
    } value;
} proven_arg_t;

/**
 * @brief Simple argument constructors.
 */
static inline proven_arg_t proven_arg_none(void) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_NONE;
    return arg;
}
/**
 * @brief A single character.
 *
 * A `char` VARIABLE now renders as a character: `char c = 'Z'; PROVEN_ARG(c)` gives `Z`.
 * It used to give `90` - `char` was mapped to the integer argument, so there was no way
 * to emit a character at all, and the ASCII column of a hex dump had to be built by hand
 * into a separate buffer and passed as a string.
 *
 * @note A character LITERAL is not a `char` in C - `'Z'` has type `int` - so
 *       `PROVEN_ARG('Z')` still renders `90`, and no amount of `_Generic` can change
 *       that. Use `proven_arg_char('Z')` when you mean the character.
 */
static inline proven_arg_t proven_arg_char(char v) {
    return (proven_arg_t){ .type = PROVEN_ARG_CHAR, .value = { .c = v } };
}

/**
 * @brief A value of your own type, rendered by your own function.
 *
 * Before this existed the formatter was a closed set: a `vec3` or a `uuid` could not be
 * printed at all. The choice was to pre-format it into a scratch string and pass THAT -
 * an allocation and a copy per value - or to give up and print the fields one by one.
 *
 * The renderer sees a sink, not a buffer, so it composes: it can call the formatter
 * again. Width, fill and alignment work on the result, because the formatter measures
 * it first. Type letters (`{:x}`, `{:f}`) are refused - the library has no idea what
 * they would mean for your type, and inventing an answer is how a formatter starts
 * lying.
 *
 * ```c
 * static proven_err_t render_point(proven_fmt_sink_t out, const void *obj) {
 *     const point_t *p = obj;
 *     proven_byte_t tmp[64];
 *     proven_u8str_t s = proven_u8str_borrow(tmp, sizeof tmp);
 *     proven_err_t e = PROVEN_FMT(&s, "({}, {})", PROVEN_ARG(p->x), PROVEN_ARG(p->y)).err;
 *     if (!proven_is_ok(e)) return e;
 *     return proven_fmt_put(out, proven_u8str_as_view(&s));
 * }
 * ...
 * PROVEN_FMT(&out, "at {:>12}", PROVEN_ARG_OF(&p, render_point));
 * ```
 */
static inline proven_arg_t proven_arg_custom(const void *obj, proven_fmt_render_fn render) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_CUSTOM;
    arg.value.custom.obj = obj;
    arg.value.custom.render = render;
    return arg;
}

/** @brief A boolean. Renders as `true` or `false`, not as 1 or 0. */
static inline proven_arg_t proven_arg_bool(bool v) {
    return (proven_arg_t){ .type = PROVEN_ARG_BOOL, .value = { .b = v } };
}

static inline proven_arg_t proven_arg_i32(int v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_I32;
    arg.value.i32 = (proven_i32)v;
    return arg;
}
static inline proven_arg_t proven_arg_u32(unsigned int v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_U32;
    arg.value.u32 = (proven_u32)v;
    return arg;
}
static inline proven_arg_t proven_arg_i64(long long v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_I64;
    arg.value.i64 = (proven_i64)v;
    return arg;
}
static inline proven_arg_t proven_arg_u64(unsigned long long v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_U64;
    arg.value.u64 = (proven_u64)v;
    return arg;
}
#ifndef PROVEN_FMT_NO_FLOAT
/**
 * @brief Floating-point argument.
 *
 * `{}` renders six fractional digits, **correctly rounded** - round-to-nearest,
 * ties-to-even on the exact binary value, through the same engine the rest of the
 * library uses. It matches `printf("%.6f")` where `printf` is itself correct.
 * (This comment used to say "round-half-up". It was wrong.)
 *
 * @note The form is not always fixed-point. Magnitudes outside the range where a
 *       six-digit fixed form would carry information switch to scientific
 *       notation: `1e20` renders as `1.000000e+20`, and `5e-7` as `5.000000e-07`
 *       rather than `printf`'s `0.000000`. That is more informative, and it is a
 *       difference worth knowing about before you diff two logs.
 * @note The spec grammar can ask for more than the default: `{:.3}` gives three
 *       decimals, `{:.0}` gives none, `{:f}` forces the fixed form, and `{:g}` gives
 *       the shortest representation that round-trips. Until v26.07.12i none of these
 *       existed - every float came out with exactly six decimals, forever, which is
 *       why a float column could not be aligned: 12.5 took nine characters and 100.0
 *       took ten, and the column broke.
 * @note The float path stays in double precision, so output is target-deterministic.
 */
static inline proven_arg_t proven_arg_f64(double v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_F64;
    arg.value.f64 = v;
    return arg;
}
#endif
/**
 * @brief Constructs a C-string argument.
 * 
 * @warning PROVEN_ARG_CSTR is NOT a fully safe API as it bounds string iteration on
 *          finding a NUL terminator. It MUST ONLY be used with trusted, NUL-terminated, 
 *          live pointers. 
 *          If a non-NUL terminated pointer or freed pointer is passed, it risks out-of-bounds 
 *          memory reads.
 *          
 *          It is highly recommended to use `proven_arg_str_view` instead by default.
 * @warning Self-aliasing C-string arguments are not supported.
 *          Do not pass a C string pointer that points inside the destination string's
 *          internal buffer. Use PROVEN_ARG_STR_VIEW for self-referential formatting.
 */
static inline proven_arg_t proven_arg_cstr(const char *v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_CSTR;
    arg.value.cstr = v;
    return arg;
}

/**
 * @brief Constructs a bounding string view argument.
 * 
 * Searches for a NUL terminator only within the given `max_len`. 
 * This provides a safer alternative to `proven_arg_cstr` for partially trusted C-strings.
 */
static inline proven_arg_t proven_arg_cstr_n(const char *v, proven_size_t max_len) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_STR_VIEW;
    if (v) {
        proven_size_t len = 0;
        while (len < max_len && v[len] != '\0') {
            len++;
        }
        arg.value.str_view.ptr = (const proven_byte_t *)v;
        arg.value.str_view.size = len;
    }
    return arg;
}

static inline proven_arg_t proven_arg_str_view(proven_u8str_view_t v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_STR_VIEW;
    arg.value.str_view = v;
    return arg;
}
static inline proven_arg_t proven_arg_datetime(proven_datetime_t v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_DATETIME;
    arg.value.datetime = v;
    return arg;
}
static inline proven_arg_t proven_arg_ptr(const void *v) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_PTR;
    arg.value.ptr = v;
    return arg;
}

/**
 * @brief Type-safe constructor for function pointers.
 */
static inline proven_arg_t proven_arg_fn(void (*v)(void)) {
    proven_arg_t arg = {0};
    arg.type = PROVEN_ARG_FN;
    arg.value.fn = v;
    return arg;
}

/**
 * @brief Helper for unsigned char strings to avoid pointer-sign warnings.
 */
static inline proven_arg_t proven_arg_ucstr(const unsigned char *v) { return proven_arg_cstr((const char *)v); }

/**
 * @brief Identity function for arguments that are already proven_arg_t.
 */
static inline proven_arg_t proven_arg_identity(proven_arg_t v) { return v; }

/**
 * @brief Safety macro for function pointers.
 * Ensures the input is treated as a function pointer without casting to void*.
 * @note This is a library extension API, as casting arbitrary function pointers
 *       to void (*)(void) operates marginally outside strict ISO C boundaries, 
 *       though widely supported on target architecture platforms.
 */
#define PROVEN_ARG_FN(f) proven_arg_fn((void (*)(void))(f))

/**
 * @brief Helper macro for safely bounding C-string arguments.
 */
#define PROVEN_ARG_CSTR_N(v, max_len) proven_arg_cstr_n(v, max_len)

/**
 * @brief A value of your own type: `PROVEN_ARG_OF(&p, render_point)`.
 *
 * `PROVEN_ARG` cannot reach user types - `_Generic` has to know every type it dispatches
 * on, and it cannot know yours. This is the door.
 */
#define PROVEN_ARG_OF(objptr, renderfn) proven_arg_custom((const void *)(objptr), (renderfn))

/**
 * @brief Type-safe argument selection using C11 _Generic.
 */
#ifndef PROVEN_FMT_NO_FLOAT
#define PROVEN_ARG(x) _Generic((x), \
    _Bool: proven_arg_bool, \
    char: proven_arg_char, \
    signed char: proven_arg_i32, \
    unsigned char: proven_arg_u32, \
    short: proven_arg_i32, \
    unsigned short: proven_arg_u32, \
    int: proven_arg_i32, \
    unsigned int: proven_arg_u32, \
    long: proven_arg_i64, \
    unsigned long: proven_arg_u64, \
    long long: proven_arg_i64, \
    unsigned long long: proven_arg_u64, \
    double: proven_arg_f64, \
    float: proven_arg_f64, \
    const char*: proven_arg_cstr, \
    char*: proven_arg_cstr, \
    unsigned char*: proven_arg_ucstr, \
    const unsigned char*: proven_arg_ucstr, \
    void*: proven_arg_ptr, \
    const void*: proven_arg_ptr, \
    proven_u8str_view_t: proven_arg_str_view, \
    proven_datetime_t: proven_arg_datetime, \
    proven_arg_t: proven_arg_identity \
)(x)
#else
#define PROVEN_ARG(x) _Generic((x), \
    _Bool: proven_arg_bool, \
    char: proven_arg_char, \
    signed char: proven_arg_i32, \
    unsigned char: proven_arg_u32, \
    short: proven_arg_i32, \
    unsigned short: proven_arg_u32, \
    int: proven_arg_i32, \
    unsigned int: proven_arg_u32, \
    long: proven_arg_i64, \
    unsigned long: proven_arg_u64, \
    long long: proven_arg_i64, \
    unsigned long long: proven_arg_u64, \
    const char*: proven_arg_cstr, \
    char*: proven_arg_cstr, \
    unsigned char*: proven_arg_ucstr, \
    const unsigned char*: proven_arg_ucstr, \
    void*: proven_arg_ptr, \
    const void*: proven_arg_ptr, \
    proven_u8str_view_t: proven_arg_str_view, \
    proven_datetime_t: proven_arg_datetime, \
    proven_arg_t: proven_arg_identity \
)(x)
#endif

/**
 * @brief Internal structural formatter engine.
 *
 * The args array is expected to include a leading proven_arg_none()
 * sentinel at index 0. User code should normally call the public
 * formatting macros instead of this function.
 * 
 * @note The formatter verifies that all provided arguments are consumed. 
 * Extra unused arguments are treated as PROVEN_ERR_INVALID_ARG.
 * 
 * @param alloc Allocator for 'grow' mode. If NULL, uses fixed capacity.
 * @param str Target string.
 * @param trunc If true, performs best-effort truncation. If false, atomic.
 */
[[nodiscard]]
proven_fmt_result_t proven_u8str_fmt_internal(proven_allocator_t alloc, proven_u8str_t *str, bool trunc, const char *fmt, proven_allocator_t scratch, const proven_arg_t *args, proven_size_t args_count);

/**
 * @brief Appends formatted content.
 * CATEGORY: Atomic Fixed-Capacity
 * 
 * Returns PROVEN_ERR_OUT_OF_BOUNDS and leaves string unchanged if capacity is insufficient.
 */
#define proven_u8str_append_fmt(str, fmt, ...) \
    proven_u8str_fmt_internal((proven_allocator_t){0}, str, false, fmt, (proven_allocator_t){0}, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

/**
 * @brief Appends formatted content, truncating if necessary.
 * CATEGORY: Best-Effort/Truncating
 * 
 * Returns actual written bytes and required bytes in proven_fmt_result_t.
 */
#define proven_u8str_append_fmt_trunc(str, fmt, ...) \
    proven_u8str_fmt_internal((proven_allocator_t){0}, str, true, fmt, (proven_allocator_t){0}, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

/**
 * @brief Appends formatted content, growing the buffer if necessary.
 * CATEGORY: Atomic Growable
 */
#define proven_u8str_append_fmt_grow(alloc, str, fmt, ...) \
    proven_u8str_fmt_internal(alloc, str, false, fmt, (proven_allocator_t){0}, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

/**
 * @brief Appends formatted content, growing the buffer if necessary, while specifying a scratch allocator for patch allocations.
 * CATEGORY: Atomic Growable (Scratch)
 */
#define proven_u8str_append_fmt_with_scratch(alloc, str, fmt, scratch, ...) \
    proven_u8str_fmt_internal(alloc, str, false, fmt, scratch, \
        ((const proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }), \
        (sizeof((proven_arg_t[]){ proven_arg_none() __VA_OPT__(,) __VA_ARGS__ }) / sizeof(proven_arg_t)))

#define PROVEN_FMT_IS_OK(res) proven_is_ok((res).err)

#endif /* PROVEN_FMT_H */
