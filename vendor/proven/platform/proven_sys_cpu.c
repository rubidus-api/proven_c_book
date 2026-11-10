#include "proven_sys_cpu.h"

#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))

bool proven_sys_cpu_bmi2(void) {
    __builtin_cpu_init();
    return __builtin_cpu_supports("bmi2");
}

#else

bool proven_sys_cpu_bmi2(void) { return false; }

#endif
