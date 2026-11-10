#ifndef PROVEN_DER_H
#define PROVEN_DER_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"

/**
 * @file der.h
 * @brief DER (X.690): a strict reader and a writer for the nesting certificates, keys and CMS
 *        use (since v0.109.0).
 *
 * The reader is the one this library's own X.509 and key parsing uses: definite lengths only,
 * each length in its shortest form, nothing past the input, one-byte tags (the high-tag-number
 * form is refused: nothing in X.509 or CMS needs it). It never allocates and never copies: every
 * view points into the input.
 *
 * The writer fills caller memory. Constructed elements are opened and closed - up to
 * PROVEN_DER_MAX_DEPTH deep - and their lengths are written when they close, the content moved
 * along when the length needs more than one byte. A SET closed with proven_der_end_set_of has its
 * elements put in DER order (X.690 11.6), as CMS's signed attributes must be. Errors are sticky:
 * the first one stops writing and is what proven_der_writer_finish returns, so a run of calls
 * needs one check at the end.
 *
 * Pure computation: no allocation, no OS; part of the freestanding profile.
 */

#define PROVEN_DER_BOOLEAN   ((proven_u8)0x01)
#define PROVEN_DER_INTEGER   ((proven_u8)0x02)
#define PROVEN_DER_BIT_STRING ((proven_u8)0x03)
#define PROVEN_DER_OCTET_STRING ((proven_u8)0x04)
#define PROVEN_DER_NULL      ((proven_u8)0x05)
#define PROVEN_DER_OID       ((proven_u8)0x06)
#define PROVEN_DER_UTF8_STRING ((proven_u8)0x0c)
#define PROVEN_DER_PRINTABLE_STRING ((proven_u8)0x13)
#define PROVEN_DER_IA5_STRING ((proven_u8)0x16)
#define PROVEN_DER_UTC_TIME  ((proven_u8)0x17)
#define PROVEN_DER_GENERALIZED_TIME ((proven_u8)0x18)
#define PROVEN_DER_SEQUENCE  ((proven_u8)0x30)
#define PROVEN_DER_SET       ((proven_u8)0x31)
/** @brief A context-specific tag: `[n]`, constructed (EXPLICIT, or IMPLICIT of a constructed type). */
#define PROVEN_DER_CONTEXT(n) ((proven_u8)(0xa0u | ((n) & 0x1fu)))
/** @brief A context-specific tag: `[n]` IMPLICIT of a primitive type. */
#define PROVEN_DER_CONTEXT_PRIMITIVE(n) ((proven_u8)(0x80u | ((n) & 0x1fu)))

/** @brief One element: its tag, its content, and the whole of it (tag and length included). */
typedef struct {
    proven_u8 tag;
    proven_mem_view_t content;
    proven_mem_view_t whole;
} proven_der_item_t;

/**
 * @brief Read the element at the front of `*in` and move `*in` past it.
 * @return PROVEN_ERR_EOF when `*in` is empty; PROVEN_ERR_INVALID_FORMAT for an indefinite or
 *         non-minimal length, a length past the input, or a high-tag-number tag;
 *         PROVEN_ERR_INVALID_ARG for null pointers. `*in` is unchanged on error.
 */
[[nodiscard]]
proven_err_t proven_der_read(proven_mem_view_t *in, proven_der_item_t *out);

/** @brief proven_der_read of an element that must have `tag`: PROVEN_ERR_INVALID_FORMAT (and
 *         `*in` unchanged) when it has another. `content` may be null. */
[[nodiscard]]
proven_err_t proven_der_expect(proven_mem_view_t *in, proven_u8 tag, proven_mem_view_t *content);

/**
 * @brief Read an INTEGER that must be non-negative and minimally encoded, giving its magnitude
 *        without the leading zero a high bit needs (zero is one byte, 00).
 * @return PROVEN_ERR_INVALID_FORMAT for a negative or non-minimal integer, or another tag.
 */
[[nodiscard]]
proven_err_t proven_der_read_uint(proven_mem_view_t *in, proven_mem_view_t *magnitude);

/** @brief An INTEGER that must fit in 64 bits unsigned. PROVEN_ERR_OVERFLOW when it does not. */
[[nodiscard]]
proven_err_t proven_der_read_u64(proven_mem_view_t *in, proven_u64 *value);

/**
 * @brief Read a BIT STRING: its bytes and how many bits of the last one are unused (0 to 7,
 *        and those bits zero, as DER requires).
 */
[[nodiscard]]
proven_err_t proven_der_read_bits(proven_mem_view_t *in, proven_mem_view_t *bytes, proven_u8 *unused_bits);

/** @brief True when `content` (an OID's content octets) is the OID whose content octets are `oid`. */
[[nodiscard]]
bool proven_der_oid_is(proven_mem_view_t content, proven_mem_view_t oid);

#define PROVEN_DER_MAX_DEPTH 16

/** @brief A writer over caller memory. Fill it with proven_der_writer_init; the fields are its own. */
typedef struct {
    proven_byte_t *buf;
    proven_size_t cap;
    proven_size_t len;
    proven_size_t open[PROVEN_DER_MAX_DEPTH];   /* where each open element's content begins */
    proven_u8 depth;
    proven_err_t err;
} proven_der_writer_t;

/** @brief Begin writing into `buf`. */
void proven_der_writer_init(proven_der_writer_t *w, proven_mem_mut_t buf);

/** @brief Open a constructed element (`PROVEN_DER_SEQUENCE`, `PROVEN_DER_SET`, `PROVEN_DER_CONTEXT(n)`,
 *         or an OCTET STRING or BIT STRING whose content is DER being written). Deeper than
 *         PROVEN_DER_MAX_DEPTH is PROVEN_ERR_OUT_OF_BOUNDS. A BIT STRING opened here gets its
 *         unused-bits byte (0) first. */
void proven_der_begin(proven_der_writer_t *w, proven_u8 tag);

/** @brief Close the innermost open element and write its length. */
void proven_der_end(proven_der_writer_t *w);

/** @brief Close the innermost open element - a SET OF - putting its elements in DER order:
 *         compared as octet strings, the shorter padded with zeros (X.690 11.6). */
void proven_der_end_set_of(proven_der_writer_t *w);

/** @brief A primitive element with this content. */
void proven_der_put(proven_der_writer_t *w, proven_u8 tag, proven_mem_view_t content);

/** @brief An INTEGER of a big-endian magnitude, written minimally (leading zeros dropped, one
 *         added when the high bit is set). An empty magnitude is zero. */
void proven_der_put_uint(proven_der_writer_t *w, proven_mem_view_t magnitude);

/** @brief An INTEGER of a 64-bit value. */
void proven_der_put_u64(proven_der_writer_t *w, proven_u64 value);

/** @brief A BIT STRING of whole bytes (no unused bits). */
void proven_der_put_bits(proven_der_writer_t *w, proven_mem_view_t bytes);

/** @brief Bytes that are already DER - a certificate, a Name, an AlgorithmIdentifier - as they are. */
void proven_der_put_raw(proven_der_writer_t *w, proven_mem_view_t der);

/**
 * @brief The end: the length written, or the first error.
 * @return PROVEN_ERR_OUT_OF_BOUNDS when the buffer was too small or too deep;
 *         PROVEN_ERR_INVALID_STATE when an element is still open or more were closed than
 *         opened; PROVEN_ERR_INVALID_FORMAT for a SET OF whose content is not elements.
 */
[[nodiscard]]
proven_err_t proven_der_writer_finish(const proven_der_writer_t *w, proven_size_t *len);

/** @brief How deep proven_der_from_ber reads: the outermost element is depth 0, and an element
 *         below depth PROVEN_DER_BER_MAX_DEPTH is refused. */
#define PROVEN_DER_BER_MAX_DEPTH 64

/**
 * @brief Rewrite one BER element - a PKCS#12 file as Windows and some other tools export it - into
 *        the DER that proven_der_read reads. Since v0.112.0.
 *
 * What it rewrites, and only this: an indefinite length becomes a definite one; every length is
 * written in its shortest form; a constructed OCTET STRING (tag 0x24, at any depth, its pieces
 * OCTET STRINGs themselves primitive or constructed) becomes one primitive OCTET STRING of its
 * pieces' content joined. Everything else is copied as it is: the order of a SET's elements,
 * a BOOLEAN's value, the constructed forms of other string types. So the result is DER for what
 * PKCS#12, PKCS#8 and CMS files contain, not a canonical form of any BER. The rewrite and the
 * inputs refused are those of the converter rubrapack used before this function, so its outputs
 * did not change.
 *
 * Refused (PROVEN_ERR_INVALID_FORMAT): anything after the one element; a high-tag-number tag; an
 * indefinite length on a primitive element; a long-form length of more than 4 bytes; a length
 * past its input; a missing end-of-contents; a piece of a constructed OCTET STRING that is not
 * one; nesting deeper than PROVEN_DER_BER_MAX_DEPTH. A length in long form that is not the
 * shortest is accepted and rewritten - that is the point.
 *
 * Writes into `out`; nothing is allocated. `*len` gets the DER size, on PROVEN_OK and on
 * PROVEN_ERR_OUT_OF_BOUNDS (`out` too small, nothing usable written), so a call with an empty
 * `out` measures. The DER can be a few bytes longer than the BER: an indefinite length over
 * 65535 bytes of content takes 5 bytes where the BER took 4. `out` must not overlap `ber`.
 * PROVEN_ERR_INVALID_ARG for a NULL `len` or an ill-formed view.
 */
[[nodiscard]]
proven_err_t proven_der_from_ber(proven_mem_view_t ber, proven_mem_mut_t out, proven_size_t *len);

#endif /* PROVEN_DER_H */
