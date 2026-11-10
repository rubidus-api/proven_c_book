#include "proven/der.h"
#include "../../platform/proven_sys_mem.h"

/* DER (X.690): the strict reader of der.h and its writer. */

proven_err_t proven_der_read(proven_mem_view_t *in, proven_der_item_t *out) {
    if (!in || !out || (in->size > 0 && !in->ptr)) return PROVEN_ERR_INVALID_ARG;
    if (in->size == 0) return PROVEN_ERR_EOF;
    const proven_byte_t *p = in->ptr;
    const proven_size_t n = in->size;
    if (n < 2 || (p[0] & 0x1fu) == 0x1fu) return PROVEN_ERR_INVALID_FORMAT;
    proven_size_t len = p[1], head = 2;
    if (len & 0x80u) {
        const proven_size_t k = len & 0x7fu;
        /* 0x80 is the indefinite form; more length bytes than a size holds cannot be in memory. */
        if (k == 0 || k > sizeof(proven_size_t) || n < 2 + k || p[2] == 0) return PROVEN_ERR_INVALID_FORMAT;
        len = 0;
        for (proven_size_t i = 0; i < k; ++i) len = (len << 8) | p[2 + i];
        if (len < 0x80u) return PROVEN_ERR_INVALID_FORMAT;          /* would have fitted in the short form */
        head = 2 + k;
    }
    if (len > n - head) return PROVEN_ERR_INVALID_FORMAT;
    out->tag = p[0];
    out->content = (proven_mem_view_t){ .ptr = p + head, .size = len };
    out->whole = (proven_mem_view_t){ .ptr = p, .size = head + len };
    in->ptr = p + head + len;
    in->size = n - head - len;
    return PROVEN_OK;
}

proven_err_t proven_der_expect(proven_mem_view_t *in, proven_u8 tag, proven_mem_view_t *content) {
    if (!in) return PROVEN_ERR_INVALID_ARG;
    proven_mem_view_t at = *in;
    proven_der_item_t it;
    proven_err_t e = proven_der_read(&at, &it);
    if (e == PROVEN_ERR_EOF) return PROVEN_ERR_INVALID_FORMAT;
    if (e != PROVEN_OK) return e;
    if (it.tag != tag) return PROVEN_ERR_INVALID_FORMAT;
    *in = at;
    if (content) *content = it.content;
    return PROVEN_OK;
}

proven_err_t proven_der_read_uint(proven_mem_view_t *in, proven_mem_view_t *magnitude) {
    if (!in || !magnitude) return PROVEN_ERR_INVALID_ARG;
    proven_mem_view_t at = *in, c;
    proven_err_t e = proven_der_expect(&at, PROVEN_DER_INTEGER, &c);
    if (e != PROVEN_OK) return e;
    if (c.size == 0 || (c.ptr[0] & 0x80u)) return PROVEN_ERR_INVALID_FORMAT;                  /* empty, or negative */
    if (c.size > 1 && c.ptr[0] == 0 && !(c.ptr[1] & 0x80u)) return PROVEN_ERR_INVALID_FORMAT;  /* a zero byte it did not need */
    if (c.size > 1 && c.ptr[0] == 0) { c.ptr++; c.size--; }
    *in = at;
    *magnitude = c;
    return PROVEN_OK;
}

proven_err_t proven_der_read_u64(proven_mem_view_t *in, proven_u64 *value) {
    if (!in || !value) return PROVEN_ERR_INVALID_ARG;
    proven_mem_view_t at = *in, m;
    proven_err_t e = proven_der_read_uint(&at, &m);
    if (e != PROVEN_OK) return e;
    if (m.size > 8) return PROVEN_ERR_OVERFLOW;
    proven_u64 v = 0;
    for (proven_size_t i = 0; i < m.size; ++i) v = (v << 8) | m.ptr[i];
    *in = at;
    *value = v;
    return PROVEN_OK;
}

proven_err_t proven_der_read_bits(proven_mem_view_t *in, proven_mem_view_t *bytes, proven_u8 *unused_bits) {
    if (!in || !bytes || !unused_bits) return PROVEN_ERR_INVALID_ARG;
    proven_mem_view_t at = *in, c;
    proven_err_t e = proven_der_expect(&at, PROVEN_DER_BIT_STRING, &c);
    if (e != PROVEN_OK) return e;
    if (c.size == 0 || c.ptr[0] > 7 || (c.size == 1 && c.ptr[0] != 0)) return PROVEN_ERR_INVALID_FORMAT;
    const proven_u8 unused = c.ptr[0];
    if (unused && (c.ptr[c.size - 1] & ((1u << unused) - 1u))) return PROVEN_ERR_INVALID_FORMAT;   /* DER: unused bits are zero */
    *in = at;
    *bytes = (proven_mem_view_t){ .ptr = c.ptr + 1, .size = c.size - 1 };
    *unused_bits = unused;
    return PROVEN_OK;
}

bool proven_der_oid_is(proven_mem_view_t content, proven_mem_view_t oid) {
    if (content.size != oid.size || (content.size > 0 && (!content.ptr || !oid.ptr))) return false;
    for (proven_size_t i = 0; i < content.size; ++i) if (content.ptr[i] != oid.ptr[i]) return false;
    return true;
}

// -----------------------------------------------------------------------------
// The writer
// -----------------------------------------------------------------------------

void proven_der_writer_init(proven_der_writer_t *w, proven_mem_mut_t buf) {
    if (!w) return;
    w->buf = buf.ptr;
    w->cap = buf.ptr ? buf.size : 0;
    w->len = 0;
    w->depth = 0;
    w->err = PROVEN_OK;
}

static bool dw_room(proven_der_writer_t *w, proven_size_t n) {
    if (w->err != PROVEN_OK) return false;
    if (n > w->cap - w->len) { w->err = PROVEN_ERR_OUT_OF_BOUNDS; return false; }
    return true;
}

/* The bytes of a length: 1 for under 128, else 1 + its significant bytes. */
static proven_size_t dw_len_size(proven_size_t len) {
    if (len < 0x80u) return 1;
    proven_size_t k = 0;
    for (proven_size_t v = len; v; v >>= 8) ++k;
    return 1 + k;
}

static void dw_len_write(proven_byte_t *at, proven_size_t len) {
    const proven_size_t size = dw_len_size(len);
    if (size == 1) { at[0] = (proven_byte_t)len; return; }
    at[0] = (proven_byte_t)(0x80u | (size - 1));
    for (proven_size_t i = 0; i < size - 1; ++i) at[size - 1 - i] = (proven_byte_t)(len >> (8 * i));
}

static void dw_head(proven_der_writer_t *w, proven_u8 tag, proven_size_t len) {
    const proven_size_t ls = dw_len_size(len);
    if (!dw_room(w, 1 + ls)) return;
    w->buf[w->len] = tag;
    dw_len_write(w->buf + w->len + 1, len);
    w->len += 1 + ls;
}

static void dw_bytes(proven_der_writer_t *w, const proven_byte_t *p, proven_size_t n) {
    if (n == 0 || !dw_room(w, n)) return;
    proven_sys_mem_copy(w->buf + w->len, p, n);
    w->len += n;
}

void proven_der_begin(proven_der_writer_t *w, proven_u8 tag) {
    if (!w || w->err != PROVEN_OK) return;
    if (w->depth >= PROVEN_DER_MAX_DEPTH) { w->err = PROVEN_ERR_OUT_OF_BOUNDS; return; }
    if (!dw_room(w, 2)) return;
    w->buf[w->len] = tag;
    w->buf[w->len + 1] = 0;                 /* the length's one byte, for now */
    w->len += 2;
    w->open[w->depth++] = w->len;
    if (tag == PROVEN_DER_BIT_STRING && dw_room(w, 1)) w->buf[w->len++] = 0;   /* no unused bits */
}

void proven_der_end(proven_der_writer_t *w) {
    if (!w || w->err != PROVEN_OK) return;
    if (w->depth == 0) { w->err = PROVEN_ERR_INVALID_STATE; return; }
    const proven_size_t start = w->open[--w->depth];
    const proven_size_t len = w->len - start;
    const proven_size_t ls = dw_len_size(len);
    if (ls > 1) {
        /* The length needs more than its byte: the content moves along to make room. */
        if (!dw_room(w, ls - 1)) return;
        proven_sys_mem_move(w->buf + start + ls - 1, w->buf + start, len);
        w->len += ls - 1;
    }
    dw_len_write(w->buf + start - 1, len);
}

/* Reverse [a, b) in place. */
static void dw_reverse(proven_byte_t *p, proven_size_t a, proven_size_t b) {
    while (a + 1 < b) {
        proven_byte_t t = p[a]; p[a] = p[b - 1]; p[b - 1] = t;
        ++a; --b;
    }
}

/* X.690 11.6: as octet strings, the shorter padded at its end with zeros. */
static int dw_order(const proven_byte_t *a, proven_size_t an, const proven_byte_t *b, proven_size_t bn) {
    const proven_size_t n = an < bn ? an : bn;
    for (proven_size_t i = 0; i < n; ++i) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    for (proven_size_t i = n; i < an; ++i) if (a[i]) return 1;
    for (proven_size_t i = n; i < bn; ++i) if (b[i]) return -1;
    return 0;
}

void proven_der_end_set_of(proven_der_writer_t *w) {
    if (!w || w->err != PROVEN_OK) return;
    if (w->depth == 0) { w->err = PROVEN_ERR_INVALID_STATE; return; }
    const proven_size_t start = w->open[w->depth - 1], end = w->len;
    /* Insertion sort by rotation: each element is moved down past the larger ones before it, the
     * move a rotation of the bytes between (three reversals) - no memory beyond the output. */
    proven_size_t sorted_end = start;
    while (sorted_end < end) {
        proven_mem_view_t rest = { .ptr = w->buf + sorted_end, .size = end - sorted_end };
        proven_der_item_t it;
        if (proven_der_read(&rest, &it) != PROVEN_OK) { w->err = PROVEN_ERR_INVALID_FORMAT; return; }
        const proven_size_t it_len = it.whole.size;
        /* Where it goes: before the first sorted element larger than it. */
        proven_size_t at = start;
        while (at < sorted_end) {
            proven_mem_view_t here = { .ptr = w->buf + at, .size = sorted_end - at };
            proven_der_item_t s;
            if (proven_der_read(&here, &s) != PROVEN_OK) { w->err = PROVEN_ERR_INVALID_FORMAT; return; }
            if (dw_order(w->buf + sorted_end, it_len, s.whole.ptr, s.whole.size) < 0) break;
            at += s.whole.size;
        }
        if (at < sorted_end) {
            dw_reverse(w->buf, at, sorted_end);
            dw_reverse(w->buf, sorted_end, sorted_end + it_len);
            dw_reverse(w->buf, at, sorted_end + it_len);
        }
        sorted_end += it_len;
    }
    proven_der_end(w);
}

void proven_der_put(proven_der_writer_t *w, proven_u8 tag, proven_mem_view_t content) {
    if (!w || w->err != PROVEN_OK) return;
    if (content.size > 0 && !content.ptr) { w->err = PROVEN_ERR_INVALID_ARG; return; }
    dw_head(w, tag, content.size);
    dw_bytes(w, content.ptr, content.size);
}

void proven_der_put_uint(proven_der_writer_t *w, proven_mem_view_t m) {
    if (!w || w->err != PROVEN_OK) return;
    if (m.size > 0 && !m.ptr) { w->err = PROVEN_ERR_INVALID_ARG; return; }
    while (m.size > 0 && m.ptr[0] == 0) { m.ptr++; m.size--; }
    static const proven_byte_t zero = 0;
    const bool pad = m.size == 0 || (m.ptr[0] & 0x80u);
    dw_head(w, PROVEN_DER_INTEGER, m.size + (pad ? 1 : 0));
    if (pad) dw_bytes(w, &zero, 1);
    dw_bytes(w, m.ptr, m.size);
}

void proven_der_put_u64(proven_der_writer_t *w, proven_u64 v) {
    proven_byte_t b[8];
    for (int i = 0; i < 8; ++i) b[i] = (proven_byte_t)(v >> (56 - 8 * i));
    proven_der_put_uint(w, (proven_mem_view_t){ .ptr = b, .size = 8 });
}

void proven_der_put_bits(proven_der_writer_t *w, proven_mem_view_t bytes) {
    if (!w || w->err != PROVEN_OK) return;
    if (bytes.size > 0 && !bytes.ptr) { w->err = PROVEN_ERR_INVALID_ARG; return; }
    static const proven_byte_t none = 0;
    dw_head(w, PROVEN_DER_BIT_STRING, bytes.size + 1);
    dw_bytes(w, &none, 1);
    dw_bytes(w, bytes.ptr, bytes.size);
}

void proven_der_put_raw(proven_der_writer_t *w, proven_mem_view_t der) {
    if (!w || w->err != PROVEN_OK) return;
    if (der.size > 0 && !der.ptr) { w->err = PROVEN_ERR_INVALID_ARG; return; }
    dw_bytes(w, der.ptr, der.size);
}

proven_err_t proven_der_writer_finish(const proven_der_writer_t *w, proven_size_t *len) {
    if (!w) return PROVEN_ERR_INVALID_ARG;
    if (len) *len = 0;
    if (w->err != PROVEN_OK) return w->err;
    if (w->depth != 0) return PROVEN_ERR_INVALID_STATE;
    if (len) *len = w->len;
    return PROVEN_OK;
}

/*
 * BER to DER (RFC-0044): the rewrite rubrapack's rp_ber_to_der does, rule for rule, so a file it
 * read before reads the same now - the same bytes out and the same inputs refused. Each element is
 * read twice when it is written: once to measure its DER content, once to write it. Recursion is
 * bounded by PROVEN_DER_BER_MAX_DEPTH.
 */
typedef struct {
    proven_byte_t *buf;   /* NULL: measure only */
    proven_size_t len;
} ber_out_t;

typedef struct {
    proven_u8 tag;
    bool constructed, indefinite;
    proven_size_t head, len;
} ber_head_t;

static bool ber_head(const proven_byte_t *p, proven_size_t n, ber_head_t *h) {
    if (n < 2) return false;
    h->tag = p[0];
    if ((h->tag & 0x1fu) == 0x1fu) return false;
    h->constructed = (h->tag & 0x20u) != 0;
    h->indefinite = false;
    h->len = 0;
    if (p[1] < 0x80u) {
        h->len = p[1];
        h->head = 2;
    } else if (p[1] == 0x80u) {
        if (!h->constructed) return false;
        h->indefinite = true;
        h->head = 2;
    } else {
        const proven_size_t k = p[1] & 0x7fu;
        if (k > 4 || n < 2 + k) return false;
        for (proven_size_t i = 0; i < k; ++i) h->len = (h->len << 8) | p[2 + i];
        h->head = 2 + k;
    }
    return h->indefinite || h->len <= n - h->head;
}

static void ber_put(ber_out_t *o, const proven_byte_t *p, proven_size_t n) {
    if (o->buf && n) proven_sys_mem_copy(o->buf + o->len, p, n);
    o->len += n;
}

static void ber_put_head(ber_out_t *o, proven_u8 tag, proven_size_t len) {
    const proven_size_t ls = dw_len_size(len);
    if (o->buf) {
        o->buf[o->len] = tag;
        dw_len_write(o->buf + o->len + 1, len);
    }
    o->len += 1 + ls;
}

static bool ber_elem(const proven_byte_t *p, proven_size_t n, unsigned depth, bool content_only, ber_out_t *o,
                     proven_size_t *used, proven_u8 *der_tag, proven_size_t *content);

/*
 * The children of a constructed element - to the end-of-contents of an indefinite one, or `len`
 * bytes - written to `o` (NULL buf: measured). `join`: each child must become an OCTET STRING and
 * only its content is kept. `*used` is the bytes read, end-of-contents included; `*size` the
 * bytes written.
 */
static bool ber_children(const proven_byte_t *p, proven_size_t n, const ber_head_t *h, unsigned depth, bool join,
                         ber_out_t *o, proven_size_t *used, proven_size_t *size) {
    const proven_size_t end = h->indefinite ? n : h->len;
    const proven_size_t start = o->len;
    proven_size_t off = 0;
    for (;;) {
        if (h->indefinite) {
            if (off + 2 <= n && p[off] == 0 && p[off + 1] == 0) { *used = off + 2; break; }
            if (off >= n) return false;
        } else if (off == end) {
            *used = off;
            break;
        }
        proven_size_t u, c;
        proven_u8 t;
        if (!ber_elem(p + off, end - off, depth + 1, join, o, &u, &t, &c)) return false;
        if (join && t != PROVEN_DER_OCTET_STRING) return false;
        off += u;
    }
    *size = o->len - start;
    return true;
}

/*
 * One element at `p` (`n` bytes available) as DER. `*used`: the BER bytes it took; `*der_tag`: its
 * DER tag; `*content`: its DER content size. Written to `o` whole, or its content only when
 * `content_only` (a piece of a constructed OCTET STRING).
 */
static bool ber_elem(const proven_byte_t *p, proven_size_t n, unsigned depth, bool content_only, ber_out_t *o,
                     proven_size_t *used, proven_u8 *der_tag, proven_size_t *content) {
    ber_head_t h;
    if (depth > PROVEN_DER_BER_MAX_DEPTH || !ber_head(p, n, &h)) return false;
    if (!h.constructed) {
        if (!content_only) ber_put_head(o, h.tag, h.len);
        ber_put(o, p + h.head, h.len);
        *used = h.head + h.len;
        *der_tag = h.tag;
        *content = h.len;
        return true;
    }
    const bool join = h.tag == 0x24u;   /* a constructed OCTET STRING becomes a primitive one */
    *der_tag = join ? PROVEN_DER_OCTET_STRING : h.tag;
    proven_size_t u, c;
    ber_out_t measure = { (proven_byte_t *)0, 0 };
    if (!ber_children(p + h.head, n - h.head, &h, depth, join, &measure, &u, &c)) return false;
    if (!content_only) ber_put_head(o, *der_tag, c);
    if (o->buf && !ber_children(p + h.head, n - h.head, &h, depth, join, o, &u, &c)) return false;
    if (!o->buf) o->len += c;
    *used = h.head + u;
    *content = c;
    return true;
}

proven_err_t proven_der_from_ber(proven_mem_view_t ber, proven_mem_mut_t out, proven_size_t *len) {
    if (!len || (ber.size > 0 && !ber.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    *len = 0;
    ber_out_t m = { (proven_byte_t *)0, 0 };
    proven_size_t used, c;
    proven_u8 t;
    if (!ber_elem(ber.ptr, ber.size, 0, false, &m, &used, &t, &c) || used != ber.size) return PROVEN_ERR_INVALID_FORMAT;
    *len = m.len;
    if (m.len > out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
    ber_out_t w = { out.ptr, 0 };
    if (!ber_elem(ber.ptr, ber.size, 0, false, &w, &used, &t, &c)) return PROVEN_ERR_INVALID_FORMAT;
    return PROVEN_OK;
}
