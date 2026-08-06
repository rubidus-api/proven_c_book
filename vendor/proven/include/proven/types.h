#ifndef PROVEN_TYPES_H
#define PROVEN_TYPES_H

/**
 * @file types.h
 * @brief Fundamental type definitions for the proven library.
 * Strictly adheres to C23 standards and the SPEC v1.12.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/*
 * <stdalign.h> makes `alignof`/`alignas` available as macros under the older
 * `-std=c2x` fallback (and C11/C17), where they are not yet keywords. In C23
 * the header is empty and these are real keywords, so including it is harmless.
 * It lives in this foundation header — which every translation unit pulls in —
 * so any `.c` using `alignof` is covered regardless of its own include list.
 */
#include <stdalign.h>

#if defined(__has_include)
#  if __has_include(<uchar.h>)
#    include <uchar.h>
#    define PROVEN_HAS_UCHAR_H 1
#  endif
#else
#  include <uchar.h>
#  define PROVEN_HAS_UCHAR_H 1
#endif

#ifndef UINTPTR_MAX
#error "proven requires uintptr_t support and assumes a conventional flat-address system model for arena range checks."
#endif

/* Fixed-width integer types (No _t suffix as per SPEC) */
typedef int8_t         proven_i8;
typedef int16_t        proven_i16;
typedef int32_t        proven_i32;
typedef int64_t        proven_i64;

typedef uint8_t        proven_u8;
#if defined(PROVEN_HAS_UCHAR_H)
typedef char16_t       proven_u16;
#else
typedef uint_least16_t proven_u16;
#endif
typedef uint32_t       proven_u32;
typedef uint64_t       proven_u64;

/**
 * @brief byte type for memory access.
 * The only type allowed to alias any object representation in C.
 */
typedef unsigned char  proven_byte_t;

/* Semantic types (With _t suffix) */

/**
 * @brief Type for sizes and indices. 
 * Matches standard size_t for optimal platform alignment.
 */
typedef size_t         proven_size_t;

#ifndef PROVEN_SIZE_MAX
#define PROVEN_SIZE_MAX ((proven_size_t)SIZE_MAX)
#endif

/**
 * @brief Type for pointer differences and offsets.
 * Matches standard ptrdiff_t.
 */
typedef ptrdiff_t      proven_ptrdiff_t;

/**
 * @brief Types for holding pointers as integers.
 */
typedef intptr_t       proven_intptr_t;
typedef uintptr_t      proven_uintptr_t;

/**
 * @brief Checked integer arithmetic.
 * Wrapped with PROVEN_ prefix to maintain isolation and ensure safety.
 * Requires C23 <stdckdint.h> or compiler-specific built-ins.
 */
#if defined(__has_include) && __has_include(<stdckdint.h>)
    #include <stdckdint.h>
    #define PROVEN_CKD_ADD(res, a, b) ckd_add(res, a, b)
    #define PROVEN_CKD_SUB(res, a, b) ckd_sub(res, a, b)
    #define PROVEN_CKD_MUL(res, a, b) ckd_mul(res, a, b)
#elif defined(__GNUC__) || defined(__clang__)
    #define PROVEN_CKD_ADD(res, a, b) __builtin_add_overflow((a), (b), (res))
    #define PROVEN_CKD_SUB(res, a, b) __builtin_sub_overflow((a), (b), (res))
    #define PROVEN_CKD_MUL(res, a, b) __builtin_mul_overflow((a), (b), (res))
#else
    // Fallback: If neither built-ins nor C23 are available, the core library 
    // explicitly fails to guard against unsafe overflow behavior on legacy or 
    // restricted compilers. MSVC support is experimental and requires modern versions 
    // with appropriate intrinsics if this path is to be bypassed.
    #error "proven requires C23 <stdckdint.h> or compiler overflow builtins (__builtin_*_overflow)."
#endif

/**
 * @brief Core error codes for the proven library.
 */
typedef enum {
    PROVEN_OK = 0,
    PROVEN_ERR_NOMEM,
    PROVEN_ERR_OUT_OF_BOUNDS,
    PROVEN_ERR_INVALID_ENCODING,
    PROVEN_ERR_INVALID_ARG,
    PROVEN_ERR_IO,
    PROVEN_ERR_NOT_FOUND,
    PROVEN_ERR_INVALID_STATE,
    PROVEN_ERR_NEED_MORE,
    PROVEN_ERR_OVERFLOW,
    PROVEN_ERR_UNSUPPORTED,
    PROVEN_ERR_AGAIN,
    PROVEN_ERR_EOF,
    PROVEN_ERR_BUSY,
    PROVEN_ERR_PERMISSION,
    PROVEN_ERR_INVALID_FORMAT
} proven_err_t;

/**
 * @brief Result wrapper for a proven_size_t.
 */
typedef struct {
    proven_err_t  err;
    proven_size_t value;
} proven_result_size_t;

#endif /* PROVEN_TYPES_H */
