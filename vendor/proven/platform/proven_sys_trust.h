#ifndef PROVEN_SYS_TRUST_H
#define PROVEN_SYS_TRUST_H

/* Where the operating system keeps its trusted root certificates.
 *
 * Two shapes, because the systems differ: on Windows the roots are in a store that is
 * enumerated through an API; everywhere else they are a PEM file at one of a few usual paths. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Called once per certificate with its DER. Return false to stop. */
typedef bool (*proven_sys_trust_visit_fn)(void *ctx, const uint8_t *der, size_t len);

/* Enumerate the system store. Returns 1 when a store was opened (whatever it held), 0 when
 * there is one and it could not be opened, and -1 on systems where the roots are a file:
 * use proven_sys_trust_bundle_path there. */
int proven_sys_trust_enumerate(proven_sys_trust_visit_fn visit, void *ctx);

/* The index-th usual location of a PEM bundle, or NULL past the last. */
const char *proven_sys_trust_bundle_path(size_t index);

#endif
