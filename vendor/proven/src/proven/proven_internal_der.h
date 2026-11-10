#ifndef PROVEN_INTERNAL_DER_H
#define PROVEN_INTERNAL_DER_H

/* A strict DER reader, shared by the certificate unit and the TLS unit's key files. Definite,
 * minimal lengths only; single-byte tags only. Internal. */

#include "proven/types.h"
#include "proven/memory.h"

typedef struct { const proven_byte_t *p; proven_size_t n; } proven_der_t;

#define DER_BOOLEAN 0x01
#define DER_INTEGER 0x02
#define DER_BITSTRING 0x03
#define DER_OCTETS 0x04
#define DER_NULL 0x05
#define DER_OID 0x06
#define DER_UTCTIME 0x17
#define DER_GENTIME 0x18
#define DER_SEQUENCE 0x30

/* Take one element off the front of `in`. Definite, minimal lengths only; single-byte tags
 * only (no certificate field uses a tag number above 30). */
static inline bool der_read(proven_der_t *in, proven_byte_t *tag, proven_der_t *body, proven_mem_view_t *whole) {
    if (in->n < 2) return false;
    const proven_byte_t *start = in->p;
    proven_byte_t t = in->p[0];
    if ((t & 0x1f) == 0x1f) return false;
    proven_size_t len = in->p[1], head = 2;
    if (len & 0x80) {
        proven_size_t count = len & 0x7f;
        if (count == 0 || count > 4 || in->n < 2 + count) return false;
        len = 0;
        for (proven_size_t i = 0; i < count; ++i) len = (len << 8) | in->p[2 + i];
        if (in->p[2] == 0) return false;                         /* a leading zero byte */
        if (len < 0x80) return false;                            /* should have been short form */
        head = 2 + count;
    }
    if (in->n - head < len) return false;
    if (tag) *tag = t;
    if (body) { body->p = in->p + head; body->n = len; }
    if (whole) { whole->ptr = start; whole->size = head + len; }
    in->p += head + len;
    in->n -= head + len;
    return true;
}

static inline bool der_expect(proven_der_t *in, proven_byte_t want, proven_der_t *body, proven_mem_view_t *whole) {
    proven_byte_t tag;
    proven_der_t save = *in;
    if (!der_read(in, &tag, body, whole) || tag != want) { *in = save; return false; }
    return true;
}

static inline bool der_is(const proven_der_t *v, const proven_byte_t *bytes, proven_size_t n) {
    if (v->n != n) return false;
    for (proven_size_t i = 0; i < n; ++i) if (v->p[i] != bytes[i]) return false;
    return true;
}

#define DER_IS(v, lit) der_is((v), (const proven_byte_t *)(lit), sizeof(lit) - 1)

/* A non-negative INTEGER's magnitude, minimal encoding required. */
static inline bool der_uint(proven_der_t *in, proven_mem_view_t *out) {
    proven_der_t b;
    if (!der_expect(in, DER_INTEGER, &b, NULL) || b.n == 0) return false;
    if (b.p[0] & 0x80) return false;                             /* negative */
    if (b.n > 1 && b.p[0] == 0 && (b.p[1] & 0x80) == 0) return false;     /* a needless zero */
    if (b.n > 1 && b.p[0] == 0) { b.p++; b.n--; }
    out->ptr = b.p; out->size = b.n;
    return true;
}

static inline bool der_small_uint(proven_der_t *in, proven_u32 *out) {
    proven_mem_view_t v;
    if (!der_uint(in, &v) || v.size > 4) return false;
    proven_u32 x = 0;
    for (proven_size_t i = 0; i < v.size; ++i) x = (x << 8) | v.ptr[i];
    *out = x;
    return true;
}

#endif
