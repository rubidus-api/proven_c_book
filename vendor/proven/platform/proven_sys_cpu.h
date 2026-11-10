#ifndef PROVEN_SYS_CPU_H
#define PROVEN_SYS_CPU_H

/* Processor features the portable code may choose a build of itself for, asked at run time:
 * the code is the same C, compiled once more with the feature allowed. Elsewhere, and in every
 * freestanding build, the answer is no and the plain build runs. */

#include <stdbool.h>

/* x86 BMI2 (shifts and rotations by a register without the flags, SHLX/SHRX/RORX): the
 * Zstandard encoder's hash and bit writing (RFC-0028 S-5). */
bool proven_sys_cpu_bmi2(void);

#endif
