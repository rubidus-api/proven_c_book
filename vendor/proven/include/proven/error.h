#ifndef PROVEN_ERROR_H
#define PROVEN_ERROR_H

#include "types.h"

/**
 * @file error.h
 * @brief Error handling primitives for the proven library.
 * Strictly avoids macros for control flow.
 */

/**
 * @brief Helper inline function to check if a result was successful.
 */
static inline int proven_is_ok(proven_err_t err) {
    return err == PROVEN_OK;
}

#define PROVEN_IS_OK(err) proven_is_ok(err)

#endif /* PROVEN_ERROR_H */
