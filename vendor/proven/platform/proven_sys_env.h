#ifndef PROVEN_PLATFORM_SYS_ENV_H
#define PROVEN_PLATFORM_SYS_ENV_H

#include "proven/types.h"

/**
 * @file proven_sys_env.h
 * @brief Platform Abstraction Layer for Environment Variables.
 */

/**
 * @brief Retrieves an environment variable.
 * @param name Null-terminated UTF-8 name of the environment variable.
 * @param out_buf Buffer to store the UTF-8 representation of the value.
 * @param buf_cap Capacity of the buffer.
 * @param out_len Pointer to store the number of bytes required/written (excluding null terminator).
 * @return PROVEN_OK if the variable was found and fit into the buffer.
 *         PROVEN_ERR_NOT_FOUND if the variable does not exist.
 *         PROVEN_ERR_OUT_OF_BOUNDS if the buffer was too small (out_len is set to required size).
 */
[[nodiscard]]
proven_err_t proven_sys_env_get(const char *name, char *out_buf, size_t buf_cap, size_t *out_len);

#endif /* PROVEN_PLATFORM_SYS_ENV_H */
