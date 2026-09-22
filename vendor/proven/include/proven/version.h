#ifndef PROVEN_VERSION_H
#define PROVEN_VERSION_H

#include <stdint.h>

// include/proven/version.h
//
// Semantic versioning - MAJOR.MINOR.PATCH - from v0.0.1 (2026-09-04):
//   PATCH  fixes and documentation; nothing a caller has to change.
//   MINOR  additions that keep every existing call compiling.
//   MAJOR  a change that breaks a caller. While MAJOR is 0 the API may still move between minors.
// Releases before v0.0.1 were numbered by date (v26.MM.DDx). PROVEN_VERSION_NUM restarted with
// the scheme, so compare it with PROVEN_VERSION_ENCODE, never against one of the old date numbers.
#define PROVEN_VERSION_MAJOR  0
#define PROVEN_VERSION_MINOR  1
#define PROVEN_VERSION_PATCH  1
#define PROVEN_VERSION_STRING "proven_c_lib-v0.1.1"

// One integer for #if: MAJOR * 1000000 + MINOR * 1000 + PATCH.
//   #if PROVEN_VERSION_NUM >= PROVEN_VERSION_ENCODE(0, 1, 0)
#define PROVEN_VERSION_ENCODE(major, minor, patch) ((major) * 1000000L + (minor) * 1000L + (patch))
#define PROVEN_VERSION_NUM    PROVEN_VERSION_ENCODE(PROVEN_VERSION_MAJOR, PROVEN_VERSION_MINOR, PROVEN_VERSION_PATCH)

#endif // PROVEN_VERSION_H
