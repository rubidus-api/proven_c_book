#ifndef PROVEN_CONFIG_H
#define PROVEN_CONFIG_H

#include "proven/float_config.h"

/*
 * Compile-time feature switches for the proven library.
 *
 * PROVEN_HARDENED extends selected debug-style validation into release builds
 * when explicitly enabled by the caller. The default remains off.
 */
#ifndef PROVEN_HARDENED
#define PROVEN_HARDENED 0
#endif

/*
 * PROVEN_CRC32_SMALL selects the CRC-32 tables. 0 (the default): slicing-by-8, eight 256-entry
 * tables (8 KiB of read-only data), about 5x faster on long inputs. 1: the single 1 KiB table,
 * one byte per step, for a target whose flash budget matters more. The output is identical.
 */
#ifndef PROVEN_CRC32_SMALL
#define PROVEN_CRC32_SMALL 0
#endif

#endif /* PROVEN_CONFIG_H */
