#ifndef PROVEN_SYS_MATH_H
#define PROVEN_SYS_MATH_H

#include <stdbool.h>

/**
 * @file proven_sys_math.h
 * @brief PAL isolation for mathematical functions and float checks.
 */

typedef double proven_sys_f64_t;

[[nodiscard]]
bool proven_sys_math_isfinite_f64(proven_sys_f64_t val);

#endif // PROVEN_SYS_MATH_H
