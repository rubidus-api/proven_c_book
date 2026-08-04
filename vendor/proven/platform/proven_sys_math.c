#include "proven_sys_math.h"
#include <math.h>

bool proven_sys_math_isfinite_f64(proven_sys_f64_t val) {
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    return isfinite(val);
#else
    // Fallback if isfinite is not available, though C23/C99 should have it
    // In a true freestanding environment with no math.h, 
    // compiler built-ins or bitwise checks would go here.
    return (val == val) && (val != INFINITY) && (val != -INFINITY);
#endif
}
