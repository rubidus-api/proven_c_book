#ifndef PROVEN_INTERNAL_ZSTD_H
#define PROVEN_INTERNAL_ZSTD_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"

/*
 * Internal: what the Zstandard decoder (zstd.c) and encoder (zstd_encode.c) share - the frame
 * checksum, the code tables and the predefined distributions of RFC 8878. Not public API.
 * Everything here is static, so a program that links only one of the two carries only what
 * that one uses.
 */

#define ZSTD_MAGIC 0xFD2FB528u
#define ZSTD_BLOCK_MAX ((proven_size_t)128 * 1024)

/* Tests: true makes the encoder use its plain build even where the processor has BMI2; false
 * asks the processor again. Output is the same either way. */
void proven_zstd_encode_force_plain_(bool on);

static inline proven_u64 le64(const proven_byte_t *p) {
    return (proven_u64)p[0] | ((proven_u64)p[1] << 8) | ((proven_u64)p[2] << 16) | ((proven_u64)p[3] << 24) |
           ((proven_u64)p[4] << 32) | ((proven_u64)p[5] << 40) | ((proven_u64)p[6] << 48) | ((proven_u64)p[7] << 56);
}

static inline proven_u32 le32(const proven_byte_t *p) {
    return (proven_u32)p[0] | ((proven_u32)p[1] << 8) | ((proven_u32)p[2] << 16) | ((proven_u32)p[3] << 24);
}

/* Inlining that the hot loops need, and the bit scans the processor has, where the compiler
 * says how to ask for them; plain C otherwise. PROVEN_ZSTD_NO_BUILTINS takes the plain C bit
 * scans everywhere, so that a test can run the code other compilers get. */
#if defined(__GNUC__) || defined(__clang__)
#define ZSTD_HOT static inline __attribute__((always_inline))
#else
#define ZSTD_HOT static inline
#endif
#if (defined(__GNUC__) || defined(__clang__)) && !defined(PROVEN_ZSTD_NO_BUILTINS)
#define ZSTD_BUILTINS 1
#else
#define ZSTD_BUILTINS 0
#endif

/* The position of the highest set bit of a nonzero value. */
static inline unsigned highbit32(proven_u32 v) {
#if ZSTD_BUILTINS
    return 31u - (unsigned)__builtin_clz(v | 1u);
#else
    static const proven_u8 DEBRUIJN[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
                                            8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return DEBRUIJN[(proven_u32)(v * 0x07C4ACDDu) >> 27];
#endif
}

typedef struct {
    proven_u64 v[4];
    proven_u64 total;
    proven_byte_t buf[32];
    unsigned held;
} proven_xxh64_t;

// -----------------------------------------------------------------------------
// XXH64, for the frame checksum
// -----------------------------------------------------------------------------

#define P1 0x9E3779B185EBCA87ull
#define P2 0xC2B2AE3D27D4EB4Full
#define P3 0x165667B19E3779F9ull
#define P4 0x85EBCA77C2B2AE63ull
#define P5 0x27D4EB2F165667C5ull

static inline proven_u64 rotl64(proven_u64 x, unsigned r) { return (x << r) | (x >> (64 - r)); }

static inline proven_u64 xxh_round(proven_u64 acc, proven_u64 in) { return rotl64(acc + in * P2, 31) * P1; }

static inline void xxh_init(proven_xxh64_t *x) {
    *x = (proven_xxh64_t){ .v = { P1 + P2, P2, 0, (proven_u64)0 - P1 } };
}

static inline void xxh_update(proven_xxh64_t *x, const proven_byte_t *p, proven_size_t n) {
    x->total += n;
    if (x->held > 0) {
        while (x->held < 32 && n > 0) { x->buf[x->held++] = *p++; n--; }
        if (x->held < 32) return;
        for (unsigned i = 0; i < 4; ++i) x->v[i] = xxh_round(x->v[i], le64(x->buf + 8 * i));
        x->held = 0;
    }
    for (; n >= 32; p += 32, n -= 32) {
        x->v[0] = xxh_round(x->v[0], le64(p));
        x->v[1] = xxh_round(x->v[1], le64(p + 8));
        x->v[2] = xxh_round(x->v[2], le64(p + 16));
        x->v[3] = xxh_round(x->v[3], le64(p + 24));
    }
    while (n > 0) { x->buf[x->held++] = *p++; n--; }
}

static inline proven_u64 xxh_digest(const proven_xxh64_t *x) {
    proven_u64 h;
    if (x->total >= 32) {
        h = rotl64(x->v[0], 1) + rotl64(x->v[1], 7) + rotl64(x->v[2], 12) + rotl64(x->v[3], 18);
        for (unsigned i = 0; i < 4; ++i) h = (h ^ xxh_round(0, x->v[i])) * P1 + P4;
    } else {
        h = x->v[2] + P5;
    }
    h += x->total;
    const proven_byte_t *p = x->buf;
    unsigned n = x->held;
    for (; n >= 8; p += 8, n -= 8) h = rotl64(h ^ xxh_round(0, le64(p)), 27) * P1 + P4;
    if (n >= 4) { h = rotl64(h ^ ((proven_u64)le32(p) * P1), 23) * P2 + P3; p += 4; n -= 4; }
    for (; n > 0; ++p, --n) h = rotl64(h ^ ((proven_u64)*p * P5), 11) * P1;
    h ^= h >> 33; h *= P2;
    h ^= h >> 29; h *= P3;
    h ^= h >> 32;
    return h;
}

// -----------------------------------------------------------------------------
// Sequences: the predefined distributions (section 3.1.1.3.2.2) and the codes of lengths
// -----------------------------------------------------------------------------

static const proven_i16 LL_DEFAULT[36] = { 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
                                           2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1 };
static const proven_i16 ML_DEFAULT[53] = { 1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1 };
static const proven_i16 OF_DEFAULT[29] = { 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1 };

static const proven_u32 LL_BASE[36] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28, 32, 40,
                                        48, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
static const proven_u8 LL_BITS[36] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3,
                                       4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
static const proven_u32 ML_BASE[53] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
                                        27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 39, 41, 43, 47, 51, 59, 67, 83, 99, 131, 259, 515,
                                        1027, 2051, 4099, 8195, 16387, 32771, 65539 };
static const proven_u8 ML_BITS[53] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

/* The largest symbols and table logs of the format: Huffman, literal lengths, match lengths, offsets. */
#define HUF_LOG_MAX 12                  /* as libzstd: RFC 8878 says 11, and nothing larger is ever written */
#define LL_MAX 35
#define ML_MAX 52
#define OF_MAX 31
#define LL_LOG 9
#define ML_LOG 9
#define OF_LOG 8

// -----------------------------------------------------------------------------
// What the decoder reads, and a dictionary's reader needs too: the bit readers and the FSE table
// descriptions (moved here from zstd.c for the dictionaries, RFC-0039 L-3)
// -----------------------------------------------------------------------------

/* One state of an FSE decoding table: the symbol it stands for, and how the next state is
 * found - `next` plus that many more bits. */
typedef struct {
    proven_u16 next;
    proven_u8 symbol;
    proven_u8 bits;
} proven_fse_t;

// -----------------------------------------------------------------------------
// Bits
// -----------------------------------------------------------------------------

/* Up to eight bytes from `at`, little-endian; bytes at or past `len` read as zero. */
static inline proven_u64 load_upto(const proven_byte_t *p, proven_size_t len, proven_size_t at) {
    if (at + 8 <= len) return le64(p + at);
    proven_u64 w = 0;
    for (proven_size_t i = 0; at + i < len && i < 8; ++i) w |= (proven_u64)p[at + i] << (8 * i);
    return w;
}

/* A stream read from its end towards its beginning (RFC 8878, section 4.1): the last byte's
 * highest set bit marks where it begins, and the bits below it are read highest first.
 *
 * Eight bytes of it are held in `c`, of which the top `used` bits have been taken; `reload`
 * moves `p` back and loads again, and must be called before more than 56 bits are taken. At
 * the beginning of the stream it cannot move back: reading on there reads zeros and takes
 * `used` past 64, which the callers treat as damage. The stream has been read exactly when
 * `p` is at its beginning and `used` is 64. */
typedef struct {
    const proven_byte_t *start, *p;
    proven_u64 c;
    unsigned used;
} proven_bits_t;

static inline bool bits_init(proven_bits_t *b, const proven_byte_t *src, proven_size_t len) {
    if (len == 0 || src[len - 1] == 0) return false;
    b->start = src;
    if (len >= 8) {
        b->p = src + len - 8;
        b->c = le64(b->p);
        b->used = 0;
    } else {
        b->p = src;
        b->c = 0;
        for (proven_size_t i = 0; i < len; ++i) b->c |= (proven_u64)src[i] << (8 * i);
        b->used = (unsigned)(8 - len) * 8;
    }
    b->used += 8 - highbit32(src[len - 1]);      /* the zeros above the marker, and the marker */
    return true;
}

/* The next `n` bits (at most 56) without taking them. */
static inline proven_u64 bits_look(const proven_bits_t *b, unsigned n) {
    /* Past 63 the stream is damaged and every caller finds out (bits_exact, bits_over); what
     * is returned meanwhile only has to stay in range - at most `n` bits - which it does. */
    return ((b->c << (b->used & 63)) >> 1) >> (63 - n);
}

static inline proven_u64 bits_read(proven_bits_t *b, unsigned n) {
    const proven_u64 v = bits_look(b, n);
    b->used += n;
    return v;
}

static inline void bits_reload(proven_bits_t *b) {
    if (b->used > 64) return;
    if (b->p >= b->start + 8) {
        b->p -= b->used >> 3;
        b->used &= 7;
    } else {
        if (b->p == b->start) return;
        proven_size_t k = b->used >> 3;
        if ((proven_size_t)(b->p - b->start) < k) k = (proven_size_t)(b->p - b->start);
        b->p -= k;
        b->used -= (unsigned)k * 8;
    }
    b->c = le64(b->p);
}

/* Whether the stream was read to its beginning and no further. */
static inline bool bits_exact(proven_bits_t *b) {
    bits_reload(b);
    return b->p == b->start && b->used == 64;
}

/* Whether it was read past its beginning. */
static inline bool bits_over(proven_bits_t *b) {
    bits_reload(b);
    return b->used > 64;
}

/* A stream read from its beginning, lowest bit first: the FSE table descriptions. Past the
 * end reads zeros; the caller compares how far it read with the length. */
typedef struct {
    const proven_byte_t *p;
    proven_size_t len, bit;
} proven_fwd_t;

static inline proven_u32 fwd_peek(const proven_fwd_t *f, unsigned n) {
    const proven_u64 w = load_upto(f->p, f->len, f->bit >> 3) >> (f->bit & 7);
    return (proven_u32)(w & (((proven_u64)1 << n) - 1));
}

// -----------------------------------------------------------------------------
// FSE tables (RFC 8878, section 4.1)
// -----------------------------------------------------------------------------

/* Read a table description: the normalised counts of up to `max_sym + 1` symbols. */
static inline bool fse_read(const proven_byte_t *p, proven_size_t len, unsigned max_sym, unsigned max_log,
                     proven_i16 *norm, unsigned *nsym, unsigned *log, proven_size_t *used) {
    proven_fwd_t f = { p, len, 0 };
    const unsigned al = fwd_peek(&f, 4) + 5;
    f.bit += 4;
    if (al > max_log) return false;
    int remaining = (1 << al) + 1, threshold = 1 << al;
    unsigned nbits = al + 1, s = 0;
    bool prev0 = false;
    while (remaining > 1 && s <= max_sym) {
        if (prev0) {
            unsigned n0 = s;
            for (;;) {
                const unsigned r = fwd_peek(&f, 2);
                f.bit += 2;
                n0 += r;
                if (r != 3) break;
                if (n0 > max_sym + 1) return false;
            }
            if (n0 > max_sym) return false;
            while (s < n0) norm[s++] = 0;
        }
        const int max = (2 * threshold - 1) - remaining;
        const int bits = (int)fwd_peek(&f, nbits);
        int count;
        if ((bits & (threshold - 1)) < max) {
            count = bits & (threshold - 1);
            f.bit += nbits - 1;
        } else {
            count = bits & (2 * threshold - 1);
            if (count >= threshold) count -= max;
            f.bit += nbits;
        }
        count--;
        remaining -= count < 0 ? -count : count;
        norm[s++] = (proven_i16)count;
        prev0 = count == 0;
        while (remaining < threshold) { nbits--; threshold >>= 1; }
        if (f.bit > 8 * len) return false;
    }
    if (remaining != 1) return false;
    *used = (f.bit + 7) >> 3;
    if (*used > len) return false;
    *nsym = s;
    *log = al;
    return true;
}

static inline bool fse_build(proven_fse_t *t, const proven_i16 *norm, unsigned nsym, unsigned al) {
    const proven_u32 size = 1u << al, mask = size - 1;
    proven_u32 high = size - 1;
    proven_u16 next[256];
    for (unsigned s = 0; s < nsym; ++s) {
        if (norm[s] == -1) {
            t[high--].symbol = (proven_u8)s;
            next[s] = 1;
        } else {
            next[s] = (proven_u16)norm[s];
        }
    }
    const proven_u32 step = (size >> 1) + (size >> 3) + 3;
    proven_u32 pos = 0;
    for (unsigned s = 0; s < nsym; ++s) {
        for (int i = 0; i < norm[s]; ++i) {
            t[pos].symbol = (proven_u8)s;
            do pos = (pos + step) & mask; while (pos > high);
        }
    }
    if (pos != 0) return false;
    for (proven_u32 u = 0; u < size; ++u) {
        const proven_u32 ns = next[t[u].symbol]++;
        const unsigned nb = al - highbit32(ns);
        t[u].bits = (proven_u8)nb;
        t[u].next = (proven_u16)((ns << nb) - size);
    }
    return true;
}


/* A Huffman table description (section 4.2.1): the weights, given directly or FSE-coded, the last
 * one - which makes the total a power of two - included. `*log` is the code's longest length. */
static inline bool huf_weights(const proven_byte_t *p, proven_size_t len, proven_u8 w[256], unsigned *count, unsigned *log_out, proven_size_t *used) {
    if (len < 1) return false;
    unsigned n = 0;
    const unsigned hb = p[0];
    if (hb >= 128) {
        n = hb - 127;
        const proven_size_t bytes = (n + 1) / 2;
        if (1 + bytes > len) return false;
        for (unsigned i = 0; i < n; ++i) w[i] = (proven_u8)(i % 2 == 0 ? p[1 + i / 2] >> 4 : p[1 + i / 2] & 15);
        *used = 1 + bytes;
    } else {
        if (hb == 0 || 1 + (proven_size_t)hb > len) return false;
        const proven_byte_t *q = p + 1;
        proven_i16 norm[256];
        unsigned nsym = 0, al = 0;
        proven_size_t hu = 0;
        if (!fse_read(q, hb, 255, 6, norm, &nsym, &al, &hu)) return false;
        proven_fse_t t[64];
        if (!fse_build(t, norm, nsym, al)) return false;
        proven_bits_t b;
        if (!bits_init(&b, q + hu, hb - hu)) return false;
        /* Two states take turns over one stream; when it runs out, the other state gives one
         * last symbol. */
        proven_u32 s1 = (proven_u32)bits_read(&b, al), s2 = (proven_u32)bits_read(&b, al);
        for (;;) {
            if (n > 253) return false;
            w[n++] = t[s1].symbol;
            s1 = t[s1].next + (proven_u32)bits_read(&b, t[s1].bits);
            if (bits_over(&b)) { w[n++] = t[s2].symbol; break; }
            if (n > 253) return false;
            w[n++] = t[s2].symbol;
            s2 = t[s2].next + (proven_u32)bits_read(&b, t[s2].bits);
            if (bits_over(&b)) { w[n++] = t[s1].symbol; break; }
        }
        *used = 1 + (proven_size_t)hb;
    }
    /* The last symbol's weight is what makes the total a power of two. */
    proven_u32 rank[HUF_LOG_MAX + 2] = { 0 }, total = 0;
    for (unsigned i = 0; i < n; ++i) {
        if (w[i] > HUF_LOG_MAX) return false;
        rank[w[i]]++;
        total += (1u << w[i]) >> 1;
    }
    if (total == 0) return false;
    const unsigned log = highbit32(total) + 1;
    if (log > HUF_LOG_MAX) return false;
    const proven_u32 rest = (1u << log) - total;
    if ((rest & (rest - 1)) != 0) return false;
    const unsigned lastw = highbit32(rest) + 1;
    w[n++] = (proven_u8)lastw;
    rank[lastw]++;
    if (rank[1] < 2 || (rank[1] & 1)) return false;
    *count = n;
    *log_out = log;
    return true;
}


// -----------------------------------------------------------------------------
// Dictionaries (RFC 8878, section 5)
// -----------------------------------------------------------------------------

#define ZSTD_DICT_MAGIC 0xEC30A437u
#define ZSTD_DICT_MIN 8                 /* the smallest dictionary the format allows */

/* A dictionary as read: its ID and content, and for a formatted one the entropy tables and repeat
 * offsets a frame starts from. The tables are kept as the format has them - the Huffman weights and
 * the normalised counts - for the decoder and the encoder to build their own tables from. */
typedef struct {
    proven_u32 id;                      /* 0: a raw dictionary, which has none */
    const proven_byte_t *content;
    proven_size_t content_len;
    bool formatted;
    proven_u32 rep[3];
    proven_u8 huf_w[256];               /* each symbol's weight, the last one's included */
    unsigned huf_n, huf_log;            /* symbols 0 to huf_n - 1; the code's longest length */
    proven_i16 norm[3][64];             /* literal lengths, offsets, match lengths */
    unsigned nsym[3], log[3];
} proven_zstd_dict_t_;

enum { ZDICT_LL, ZDICT_OF, ZDICT_ML };

/* Read a dictionary: formatted when it begins with the magic number, raw otherwise. Returns
 * PROVEN_ERR_INVALID_ARG for fewer than 8 bytes, PROVEN_ERR_INVALID_FORMAT for a formatted one
 * that is damaged: tables that do not read, a repeat offset of 0 or past the content. */
static inline proven_err_t zstd_dict_read(proven_mem_view_t dict, proven_zstd_dict_t_ *out) {
    if (!out || (dict.size > 0 && !dict.ptr) || dict.size < ZSTD_DICT_MIN) return PROVEN_ERR_INVALID_ARG;
    out->formatted = le32(dict.ptr) == ZSTD_DICT_MAGIC;
    if (!out->formatted) {
        out->id = 0;
        out->content = dict.ptr;
        out->content_len = dict.size;
        out->rep[0] = 1; out->rep[1] = 4; out->rep[2] = 8;
        return PROVEN_OK;
    }
    const proven_byte_t *p = dict.ptr;
    const proven_size_t n = dict.size;
    out->id = le32(p + 4);
    proven_size_t at = 8, used = 0;
    if (!huf_weights(p + at, n - at, out->huf_w, &out->huf_n, &out->huf_log, &used)) return PROVEN_ERR_INVALID_FORMAT;
    at += used;
    static const unsigned ORDER[3] = { ZDICT_OF, ZDICT_ML, ZDICT_LL };
    static const unsigned MAXSYM[3] = { LL_MAX, OF_MAX, ML_MAX }, MAXLOG[3] = { LL_LOG, OF_LOG, ML_LOG };
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned k = ORDER[i];
        proven_fse_t t[1u << LL_LOG];
        if (!fse_read(p + at, n - at, MAXSYM[k], MAXLOG[k], out->norm[k], &out->nsym[k], &out->log[k], &used) ||
            !fse_build(t, out->norm[k], out->nsym[k], out->log[k])) return PROVEN_ERR_INVALID_FORMAT;
        at += used;
    }
    if (n - at < 12) return PROVEN_ERR_INVALID_FORMAT;
    out->content = p + at + 12;
    out->content_len = n - at - 12;
    for (unsigned i = 0; i < 3; ++i) {
        out->rep[i] = le32(p + at + 4 * i);
        if (out->rep[i] == 0 || out->rep[i] > out->content_len) return PROVEN_ERR_INVALID_FORMAT;
    }
    return PROVEN_OK;
}

#endif
