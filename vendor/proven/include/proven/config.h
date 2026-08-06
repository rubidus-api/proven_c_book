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

#endif /* PROVEN_CONFIG_H */
