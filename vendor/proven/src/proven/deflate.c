#include "proven/deflate.h"
#include "proven/hash.h"

/* DEFLATE (RFC 1951) with its two wrappers (RFC 1950, RFC 1952).
 *
 * The decompressor is a state machine that can stop anywhere - between two bits of a code if
 * that is where the input ends, in the middle of a copy if that is where the output ends -
 * and go on from there. What it accepts is what zlib accepts: the corner cases were settled
 * by trying them on zlib first, and each has a test with zlib's verdict beside it. */

#define WINDOW 32768u
#define MAXBITS 15
#define MAXLCODES 286
#define MAXDCODES 30
#define MAXCODES (MAXLCODES + MAXDCODES)

#define LFAST_BITS 9                    /* a literal/length code this short is found in one look */
#define DFAST_BITS 8                    /* and a distance code this short */
#define CFAST_BITS 7                    /* the code for the code lengths: none of its codes is longer */

/* A canonical Huffman code, in two forms. For input that is there in plenty: a table indexed
 * by the next `fbits` bits, lowest first, each entry a symbol and its code's length
 * (symbol << 4 | length) - or 0 where the code is longer than the index, or there is none.
 * For the rest - a longer code, the last bits of what a call was given - how many codes there
 * are of each length and the symbols in the order their codes come: walked a bit at a time
 * (Mark Adler's puff), which can stop between any two bits and go on in the next call. */
typedef struct {
    const proven_u16 *count;            /* MAXBITS + 1 of them */
    const proven_u16 *symbol;
    const proven_u16 *fast;             /* 1 << fbits of them; null when fbits is 0 */
    int longest;                        /* the longest code there is: bits that match nothing by then never will */
    unsigned fbits;
} proven_huff_t;

#include "deflate_tables.h"             /* the fixed code, generated: nothing is built for it when a decompressor is made */
static const proven_huff_t FIXED_LEN = { deflate_fixed_lcount, deflate_fixed_lsymbol, deflate_fixed_lfast, 9, 9 };
static const proven_huff_t FIXED_DIST = { deflate_fixed_dcount, deflate_fixed_dsymbol, deflate_fixed_dfast, 5, 5 };

enum {
    S_HEADER, S_BLOCK, S_STORED_LEN, S_STORED, S_TABLE, S_CODELENS, S_LENS, S_LENS_REPEAT,
    S_SYMBOL, S_HELD, S_LEN_EXTRA, S_DIST, S_DIST_EXTRA, S_COPY, S_TRAILER, S_DONE
};

struct proven_inflate {
    proven_allocator_t alloc;
    proven_deflate_format_t format;
    int state;
    proven_err_t error;
    bool last;                          /* the block being read is the final one */
    proven_u32 bitbuf;                  /* bits not yet used, the next one lowest */
    unsigned bitcnt;
    /* a Huffman decode in progress */
    int code, first, index, len;
    /* the header of a dynamic block */
    unsigned nlen, ndist, ncode, have;
    unsigned repeat_sym, repeat_len;
    proven_u16 lengths[MAXCODES];
    proven_huff_t lencode, distcode;           /* a dynamic block's: they point into the six arrays that follow */
    proven_u16 lcount[MAXBITS + 1], dcount[MAXBITS + 1];
    proven_u16 lsymbol[288], dsymbol[32];
    proven_u16 lfast[1 << LFAST_BITS], dfast[1 << DFAST_BITS];
    const proven_huff_t *lc, *dc;
    /* a match or a stored block in progress, or a literal decoded when the output was full */
    unsigned length, distance, extra;
    proven_byte_t held;
    proven_u32 stored;
    /* wrapper */
    unsigned head_step, head_flags, head_need;
    proven_u32 head_crc, check, trailer[2];
    proven_u64 total;
    /* History: the last 32 KiB that earlier calls produced. What the call in progress has
     * produced is in its own output, and is copied here once, when the call ends. */
    proven_u32 wpos, whave;
    proven_byte_t window[WINDOW];
};

/* Build a code from the lengths of `n` symbols, into the arrays given (`fast` and `fbits` may
 * be null and 0: no table). Returns 0 for a complete code, a positive number for an incomplete
 * one and a negative one for an over-subscribed one, which is then not to be used. */
static int construct(proven_huff_t *h, proven_u16 *count, proven_u16 *symbol, proven_u16 *fast, unsigned fbits, const proven_u16 *length, int n) {
    proven_u16 offs[MAXBITS + 1], next[MAXBITS + 1];
    h->count = count; h->symbol = symbol; h->fast = fast; h->fbits = fbits;
    for (int len = 0; len <= MAXBITS; ++len) count[len] = 0;
    for (int sym = 0; sym < n; ++sym) count[length[sym]]++;
    h->longest = 0;
    for (int len = 1; len <= MAXBITS; ++len) if (count[len]) h->longest = len;
    for (unsigned i = 0; i < (fbits ? 1u << fbits : 0u); ++i) fast[i] = 0;
    if (count[0] == n) return 0;                    /* no codes at all: complete, and any use of it fails */
    int left = 1;
    for (int len = 1; len <= MAXBITS; ++len) {
        left <<= 1;
        left -= count[len];
        if (left < 0) return left;
    }
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; ++len) offs[len + 1] = (proven_u16)(offs[len] + count[len]);
    for (int sym = 0; sym < n; ++sym) if (length[sym] != 0) symbol[offs[length[sym]]++] = (proven_u16)sym;
    if (fbits == 0) return left;
    /* The table. Codes of one length are consecutive numbers, in the order of their symbols,
     * and the first of each length follows from the counts of the shorter ones. A code is
     * sent highest bit first and bits are read lowest first, so it is its mirror image that
     * the next bits show - at every index whose low bits are that image. */
    unsigned code = 0;
    for (int len = 1; len <= MAXBITS; ++len) {
        code = (code + (len > 1 ? count[len - 1] : 0u)) << 1;
        next[len] = (proven_u16)code;
    }
    for (int sym = 0; sym < n; ++sym) {
        const unsigned len = length[sym];
        if (len == 0) continue;
        unsigned c = next[len]++;
        if (len > fbits) continue;
        unsigned image = 0;
        for (unsigned b = 0; b < len; ++b) { image = (image << 1) | (c & 1u); c >>= 1; }
        for (unsigned i = image; i < (1u << fbits); i += 1u << len) fast[i] = (proven_u16)(((unsigned)sym << 4) | len);
    }
    return left;
}

/* zlib's rule for the literal/length and distance codes: an incomplete code is accepted only
 * when it is a single code of length one. */
static bool usable(const proven_huff_t *h, int left) {
    if (left < 0) return false;
    if (left == 0) return true;
    for (int len = 2; len <= MAXBITS; ++len) if (h->count[len] != 0) return false;
    return h->count[1] == 1;
}

static void start(proven_inflate_t *z) {
    z->state = z->format == PROVEN_DEFLATE_RAW ? S_BLOCK : S_HEADER;
    z->error = PROVEN_OK;
    z->last = false;
    z->bitbuf = 0; z->bitcnt = 0;
    z->code = z->first = z->index = 0; z->len = 1;
    z->head_step = 0; z->head_flags = 0; z->head_need = 0; z->head_crc = 0;
    z->check = z->format == PROVEN_DEFLATE_ZLIB ? 1u : 0u;
    z->total = 0;
    z->wpos = 0; z->whave = 0;
    z->have = 0;
}

proven_err_t proven_inflate_create(proven_allocator_t alloc, proven_deflate_format_t format, proven_inflate_t **out) {
    if (!out) return PROVEN_ERR_INVALID_ARG;
    *out = NULL;
    if (!proven_alloc_is_valid(alloc) || (unsigned)format > (unsigned)PROVEN_DEFLATE_GZIP) return PROVEN_ERR_INVALID_ARG;
    proven_result_mem_mut_t m = alloc.alloc_fn(alloc.ctx, sizeof(proven_inflate_t), 16);
    if (m.err != PROVEN_OK) return PROVEN_ERR_NOMEM;
    proven_inflate_t *z = (proven_inflate_t *)(void *)m.value.ptr;
    z->alloc = alloc;
    z->format = format;
    start(z);
    *out = z;
    return PROVEN_OK;
}

void proven_inflate_destroy(proven_inflate_t *z) {
    if (z) z->alloc.free_fn(z->alloc.ctx, z);
}

void proven_inflate_reset(proven_inflate_t *z) {
    if (z) start(z);
}

void proven_inflate_restart(proven_inflate_t *z) {
    if (!z) return;
    const proven_u32 wpos = z->wpos, whave = z->whave;
    start(z);
    z->wpos = wpos;
    z->whave = whave;
}

/* What one call works on. */
typedef struct {
    const proven_byte_t *in;
    proven_size_t in_len, in_at;
    proven_byte_t *out;
    proven_size_t out_len, out_at;
    proven_size_t summed;               /* how much of `out` is already in the checksum */
} proven_io_t;

/* Make at least `n` bits available. False when the input ran out first. */
static bool need(proven_inflate_t *z, proven_io_t *io, unsigned n) {
    while (z->bitcnt < n) {
        if (io->in_at == io->in_len) return false;
        z->bitbuf |= (proven_u32)io->in[io->in_at++] << z->bitcnt;
        z->bitcnt += 8;
    }
    return true;
}

static proven_u32 take(proven_inflate_t *z, unsigned n) {
    const proven_u32 v = z->bitbuf & ((1u << n) - 1u);
    z->bitbuf >>= n;
    z->bitcnt -= n;
    return v;
}

/* The byte `distance` back from where the next one goes: in this call's output, or - past
 * its beginning - in the window. */
static proven_byte_t back(const proven_inflate_t *z, const proven_io_t *io, proven_size_t distance) {
    return distance <= io->out_at ? io->out[io->out_at - distance]
                                  : z->window[(z->wpos - (proven_u32)(distance - io->out_at)) & (WINDOW - 1)];
}

/* The call is over: what it produced becomes history. */
static void keep(proven_inflate_t *z, const proven_io_t *io) {
    const proven_size_t n = io->out_at;
    if (n >= WINDOW) {
        const proven_byte_t *last = io->out + n - WINDOW;
        for (proven_size_t i = 0; i < WINDOW; ++i) z->window[i] = last[i];
        z->wpos = 0;
        z->whave = WINDOW;
    } else if (n > 0) {
        const proven_size_t first = WINDOW - z->wpos < n ? WINDOW - z->wpos : n;
        for (proven_size_t i = 0; i < first; ++i) z->window[z->wpos + i] = io->out[i];
        for (proven_size_t i = first; i < n; ++i) z->window[i - first] = io->out[i];
        z->wpos = (z->wpos + (proven_u32)n) & (WINDOW - 1);
        z->whave = z->whave + n < WINDOW ? z->whave + (proven_u32)n : WINDOW;
    }
    z->total += n;
}

static proven_u64 load64(const proven_byte_t *p) {
    return (proven_u64)p[0] | ((proven_u64)p[1] << 8) | ((proven_u64)p[2] << 16) | ((proven_u64)p[3] << 24) |
           ((proven_u64)p[4] << 32) | ((proven_u64)p[5] << 40) | ((proven_u64)p[6] << 48) | ((proven_u64)p[7] << 56);
}

/* Decode one symbol of `h`, a bit at a time, picking up where the last call stopped.
 * Returns the symbol, -1 when the input ran out, -2 when the bits are no code at all. */
static int decode(proven_inflate_t *z, proven_io_t *io, const proven_huff_t *h) {
    for (; z->len <= h->longest; ++z->len) {
        if (!need(z, io, 1)) return -1;
        z->code |= (int)take(z, 1);
        const int count = h->count[z->len];
        if (z->code - count < z->first) {
            const int sym = h->symbol[z->index + (z->code - z->first)];
            z->code = z->first = z->index = 0; z->len = 1;
            return sym;
        }
        z->index += count;
        z->first += count;
        z->first <<= 1;
        z->code <<= 1;
    }
    return -2;
}

static const proven_u16 LEN_BASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const proven_u8 LEN_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const proven_u16 DIST_BASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const proven_u8 DIST_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static const proven_u8 CODE_ORDER[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static proven_err_t bad(proven_inflate_t *z, proven_err_t e) {
    z->error = e;
    return e;
}

/* The zlib or gzip header, a byte at a time. True when it is complete; false with z->error
 * unset when more input is needed. */
static bool header(proven_inflate_t *z, proven_io_t *io) {
    for (;;) {
        if (io->in_at == io->in_len) return false;
        const proven_byte_t b = io->in[io->in_at];
        const unsigned step = z->head_step;
        if (z->format == PROVEN_DEFLATE_ZLIB) {
            io->in_at++;
            if (step == 0) {
                /* CMF: method 8 (deflate) and a window of at most 32 KiB. */
                if ((b & 0x0f) != 8 || (b >> 4) > 7) { (void)bad(z, PROVEN_ERR_INVALID_FORMAT); return false; }
                z->head_flags = b;
                z->head_step = 1;
                continue;
            }
            if (((z->head_flags << 8) | b) % 31 != 0) { (void)bad(z, PROVEN_ERR_INVALID_FORMAT); return false; }
            if (b & 0x20) { (void)bad(z, PROVEN_ERR_UNSUPPORTED); return false; }      /* a preset dictionary */
            return true;
        }
        /* gzip. Steps 0-9: the fixed ten bytes. Then, as the flags say: the extra field
         * (two length bytes, then that many), the name and the comment (each to a zero
         * byte), and two bytes of the header's own CRC. */
        const proven_byte_t one[1] = { b };
        bool counted = true;                                         /* does this byte go into the header CRC? */
        io->in_at++;
        if (step < 10) {
            static const proven_byte_t magic[3] = { 0x1f, 0x8b, 8 };
            if (step < 3 && b != magic[step]) { (void)bad(z, PROVEN_ERR_INVALID_FORMAT); return false; }
            if (step == 3) {
                if (b & 0xe0) { (void)bad(z, PROVEN_ERR_INVALID_FORMAT); return false; }    /* reserved flag bits */
                z->head_flags = b;
            }
            z->head_step = step + 1;
            if (z->head_step == 10) z->head_step = (z->head_flags & 4) ? 10 : (z->head_flags & 8) ? 13 : (z->head_flags & 16) ? 14 : (z->head_flags & 2) ? 15 : 17;
        } else if (step == 10) {
            z->head_need = b; z->head_step = 11;
        } else if (step == 11) {
            z->head_need |= (unsigned)b << 8;
            z->head_step = z->head_need ? 12 : (z->head_flags & 8) ? 13 : (z->head_flags & 16) ? 14 : (z->head_flags & 2) ? 15 : 17;
        } else if (step == 12) {
            if (--z->head_need == 0) z->head_step = (z->head_flags & 8) ? 13 : (z->head_flags & 16) ? 14 : (z->head_flags & 2) ? 15 : 17;
        } else if (step == 13) {
            if (b == 0) z->head_step = (z->head_flags & 16) ? 14 : (z->head_flags & 2) ? 15 : 17;
        } else if (step == 14) {
            if (b == 0) z->head_step = (z->head_flags & 2) ? 15 : 17;
        } else if (step == 15) {
            counted = false;
            if (b != (z->head_crc & 0xff)) { (void)bad(z, PROVEN_ERR_INVALID_FORMAT); return false; }
            z->head_step = 16;
        } else {
            counted = false;
            if (b != ((z->head_crc >> 8) & 0xff)) { (void)bad(z, PROVEN_ERR_INVALID_FORMAT); return false; }
            z->head_step = 17;
        }
        if (counted) z->head_crc = proven_crc32_update(z->head_crc, (proven_mem_view_t){ .ptr = one, .size = 1 });
        if (z->head_step == 17) return true;
    }
}

static proven_u32 adler32(proven_u32 adler, const proven_byte_t *p, proven_size_t n) {
    proven_u32 a = adler & 0xffff, b = adler >> 16;
    while (n > 0) {
        proven_size_t chunk = n < 5552 ? n : 5552;                   /* the most that cannot overflow 32 bits */
        n -= chunk;
        while (chunk-- > 0) { a += *p++; b += a; }
        a %= 65521u; b %= 65521u;
    }
    return (b << 16) | a;
}

/* Bring the checksum up to date with what this call has produced so far. */
static void sum_output(proven_inflate_t *z, proven_io_t *io) {
    const proven_size_t n = io->out_at - io->summed;
    if (n == 0) return;   /* before the sum below: an empty output may be a NULL buffer */
    const proven_byte_t *p = io->out + io->summed;
    if (z->format == PROVEN_DEFLATE_ZLIB) z->check = adler32(z->check, p, n);
    else if (z->format == PROVEN_DEFLATE_GZIP) z->check = proven_crc32_update(z->check, (proven_mem_view_t){ .ptr = p, .size = n });
    io->summed = io->out_at;
}

/* The code lengths of a dynamic block, as many as can be read whole - the same bargain as
 * fast() below, and the same promise about the bits it leaves: a length that is cut by the end
 * of the input, or wrong, is left for the code that works a bit at a time. Two or three hundred
 * lengths precede every dynamic block, and for a short message they were most of the work. */
static void fast_lengths(proven_inflate_t *z, proven_io_t *io) {
    const proven_u16 *const f = z->lencode.fast;
    const proven_byte_t *const in = io->in;
    const proven_size_t in_len = io->in_len;
    const unsigned total = z->nlen + z->ndist;
    proven_size_t ia = io->in_at;
    proven_u64 bits = z->bitbuf;
    unsigned have = z->bitcnt;
    while (z->have < total) {
        if (in_len - ia >= 8) {
            bits |= load64(in + ia) << have;
            ia += (63 - have) >> 3;
            have |= 56;
        } else {
            while (have <= 56 && ia < in_len) { bits |= (proven_u64)in[ia++] << have; have += 8; }
        }
        const unsigned e = f[bits & ((1u << CFAST_BITS) - 1u)], len = e & 15u, sym = e >> 4;
        if (len == 0 || len > have) break;
        if (sym < 16) {
            z->lengths[z->have++] = (proven_u16)sym;
            bits >>= len; have -= len;
            continue;
        }
        /* 16: the previous length 3 to 6 times. 17 and 18: zero, 3 to 10 or 11 to 138 times. */
        const unsigned extra = sym == 16 ? 2 : sym == 17 ? 3 : 7;
        if (len + extra > have) break;
        unsigned times = (unsigned)((bits >> len) & ((1u << extra) - 1u)) + (sym == 18 ? 11u : 3u);
        proven_u16 value = 0;
        if (sym == 16) {
            if (z->have == 0) break;
            value = z->lengths[z->have - 1];
        }
        if (z->have + times > total) break;
        bits >>= len + extra; have -= len + extra;
        while (times-- > 0) z->lengths[z->have++] = value;
    }
    ia -= have >> 3;
    have &= 7u;
    z->bitbuf = (proven_u32)bits & ((1u << have) - 1u);
    z->bitcnt = have;
    io->in_at = ia;
}

/* Literals and matches, as many as can be decoded whole from what is there: each symbol is
 * found by one look in a table, and nothing is taken until all of it - a length, its extra
 * bits, a distance, its extra bits - is known to be present, to be valid and to fit. Whatever
 * is not - a code longer than the table's index, a symbol the input ends in the middle of, a
 * match the output has no room for, anything that is an error - is left exactly where it was
 * for the code that works a bit at a time, which is the one that can stop anywhere and the one
 * that names errors. Called with fewer than eight bits held; leaves fewer than eight, having
 * given back every whole byte it read and did not use, so that the two agree on where the
 * input is. Sets S_BLOCK when it has taken the end of the block. */
static void fast(proven_inflate_t *z, proven_io_t *io) {
    const proven_u16 *const lf = z->lc->fast, *const df = z->dc->fast;
    const proven_u32 lmask = (1u << z->lc->fbits) - 1u, dmask = (1u << z->dc->fbits) - 1u;
    const proven_byte_t *const in = io->in;
    proven_byte_t *const out = io->out;
    const proven_size_t in_len = io->in_len, out_len = io->out_len;
    proven_size_t ia = io->in_at, oa = io->out_at;
    proven_u64 bits = z->bitbuf;
    unsigned have = z->bitcnt;
    for (;;) {
        /* Up to 56 bits and more: a match is at most 15 + 5 + 15 + 13 of them. */
        if (in_len - ia >= 8) {
            bits |= load64(in + ia) << have;
            ia += (63 - have) >> 3;
            have |= 56;
        } else {
            while (have <= 56 && ia < in_len) { bits |= (proven_u64)in[ia++] << have; have += 8; }
        }
        const unsigned e = lf[bits & lmask], len = e & 15u, sym = e >> 4;
        if (len == 0 || len > have) break;
        if (sym < 256) {
            if (oa == out_len) break;
            out[oa++] = (proven_byte_t)sym;
            bits >>= len; have -= len;
            continue;
        }
        if (sym == 256) {
            bits >>= len; have -= len;
            z->state = S_BLOCK;
            break;
        }
        if (sym >= 257 + 29) break;
        /* A match: on a copy of the bits, kept only when it is whole. */
        proven_u64 b = bits >> len;
        unsigned h = have - len;
        unsigned extra = LEN_EXTRA[sym - 257];
        if (extra > h) break;
        proven_size_t length = LEN_BASE[sym - 257] + (unsigned)(b & ((1u << extra) - 1u));
        b >>= extra; h -= extra;
        const unsigned de = df[b & dmask], dlen = de & 15u, dsym = de >> 4;
        if (dlen == 0 || dlen > h || dsym >= MAXDCODES) break;
        b >>= dlen; h -= dlen;
        extra = DIST_EXTRA[dsym];
        if (extra > h) break;
        const proven_size_t distance = DIST_BASE[dsym] + (unsigned)(b & ((1u << extra) - 1u));
        b >>= extra; h -= extra;
        if (distance > z->whave + oa || length > out_len - oa) break;
        bits = b; have = h;
        if (distance > oa) {
            /* It begins before this call's output: in the window. */
            const proven_size_t before = distance - oa;
            const proven_size_t n = before < length ? before : length;
            const proven_u32 at = (z->wpos - (proven_u32)before) & (WINDOW - 1);
            const proven_size_t first = WINDOW - at < n ? WINDOW - at : n;      /* as far as the window's end; the rest from its beginning */
            for (proven_size_t i = 0; i < first; ++i) out[oa + i] = z->window[at + i];
            for (proven_size_t i = first; i < n; ++i) out[oa + i] = z->window[i - first];
            oa += n;
            length -= n;
        }
        /* The rest from the output itself. When the distance is shorter than the length the
         * copy reads what it has just written, and that is what it means: so byte by byte,
         * unless the two are far enough apart for eight at a time not to overlap. Eight at a
         * time writes up to seven bytes beyond the match - inside the output, which is why
         * room for them is asked for, and usually under what the next symbol writes; when the
         * call ends there instead, they are bytes of `out` beyond what it reports produced.
         * The header says that this may happen. Stopping at the match's last byte instead
         * cost a quarter of the speed on text, whose matches are mostly shorter than eight. */
        const proven_byte_t *from = out + oa - distance;
        if (distance >= 8 && out_len - oa >= length + 8) {
            for (proven_size_t i = 0; i < length; i += 8) {
                const proven_u64 w = load64(from + i);
                proven_byte_t *to = out + oa + i;
                to[0] = (proven_byte_t)w; to[1] = (proven_byte_t)(w >> 8); to[2] = (proven_byte_t)(w >> 16); to[3] = (proven_byte_t)(w >> 24);
                to[4] = (proven_byte_t)(w >> 32); to[5] = (proven_byte_t)(w >> 40); to[6] = (proven_byte_t)(w >> 48); to[7] = (proven_byte_t)(w >> 56);
            }
        } else {
            for (proven_size_t i = 0; i < length; ++i) out[oa + i] = from[i];
        }
        oa += length;
    }
    /* Whole bytes that were read ahead go back; the bits of the byte in use stay. */
    ia -= have >> 3;
    have &= 7u;
    z->bitbuf = (proven_u32)bits & ((1u << have) - 1u);
    z->bitcnt = have;
    io->in_at = ia;
    io->out_at = oa;
}

proven_err_t proven_inflate(proven_inflate_t *z, proven_mem_view_t in, proven_size_t *consumed,
                            proven_mem_mut_t out, proven_size_t *produced, bool *done) {
    if (consumed) *consumed = 0;
    if (produced) *produced = 0;
    if (done) *done = false;
    if (!z || !consumed || !produced || !done || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (z->error != PROVEN_OK) return z->error;
    proven_io_t io = { .in = in.ptr, .in_len = in.size, .in_at = 0, .out = out.ptr, .out_len = out.size, .out_at = 0, .summed = 0 };
    proven_err_t result = PROVEN_OK;
    bool going = true;
    while (going) {
        switch (z->state) {
            case S_HEADER:
                if (!header(z, &io)) { going = false; result = z->error; break; }
                z->state = S_BLOCK;
                break;
            case S_BLOCK:
                if (z->last) {
                    /* What is left of the last byte is padding; the trailer starts on a byte. */
                    z->bitbuf = 0; z->bitcnt = 0;
                    z->have = 0;
                    z->state = z->format == PROVEN_DEFLATE_RAW ? S_DONE : S_TRAILER;
                    break;
                }
                if (!need(z, &io, 3)) { going = false; break; }
                z->last = take(z, 1) != 0;
                switch (take(z, 2)) {
                    case 0:
                        z->bitbuf = 0; z->bitcnt = 0;               /* a stored block starts on a byte */
                        z->state = S_STORED_LEN;
                        break;
                    case 1:
                        z->lc = &FIXED_LEN; z->dc = &FIXED_DIST;
                        z->state = S_SYMBOL;
                        break;
                    case 2:
                        z->state = S_TABLE;
                        break;
                    default:
                        result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false;
                        break;
                }
                break;
            case S_STORED_LEN: {
                if (!need(z, &io, 32)) { going = false; break; }
                const proven_u32 v = z->bitbuf;
                z->bitbuf = 0; z->bitcnt = 0;
                if ((v & 0xffff) != ((~v >> 16) & 0xffff)) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                z->stored = v & 0xffff;
                z->state = S_STORED;
                break; }
            case S_STORED:
                {
                    proven_size_t n = z->stored;
                    if (n > io.in_len - io.in_at) n = io.in_len - io.in_at;
                    if (n > io.out_len - io.out_at) n = io.out_len - io.out_at;
                    for (proven_size_t i = 0; i < n; ++i) io.out[io.out_at + i] = io.in[io.in_at + i];
                    io.in_at += n; io.out_at += n;
                    z->stored -= (proven_u32)n;
                }
                if (z->stored == 0) z->state = S_BLOCK;
                else going = false;
                break;
            case S_TABLE:
                if (!need(z, &io, 14)) { going = false; break; }
                z->nlen = take(z, 5) + 257;
                z->ndist = take(z, 5) + 1;
                z->ncode = take(z, 4) + 4;
                if (z->nlen > MAXLCODES || z->ndist > MAXDCODES) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                z->have = 0;
                for (int i = 0; i < 19; ++i) z->lengths[i] = 0;
                z->state = S_CODELENS;
                break;
            case S_CODELENS:
                while (z->have < z->ncode) {
                    if (!need(z, &io, 3)) { going = false; break; }
                    z->lengths[CODE_ORDER[z->have++]] = (proven_u16)take(z, 3);
                }
                if (!going) break;
                /* The code for the code lengths must be complete: nothing is forgiven here. */
                /* (Its table is the distance code's, which is not built until these lengths are read.) */
                if (construct(&z->lencode, z->lcount, z->lsymbol, z->dfast, CFAST_BITS, z->lengths, 19) != 0) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                z->have = 0;
                z->state = S_LENS;
                break;
            case S_LENS:
                if (z->len == 1 && z->bitcnt < 8) fast_lengths(z, &io);
                while (z->have < z->nlen + z->ndist) {
                    const int sym = decode(z, &io, &z->lencode);
                    if (sym == -1) { going = false; break; }
                    if (sym < 0) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                    if (sym < 16) { z->lengths[z->have++] = (proven_u16)sym; continue; }
                    z->repeat_sym = (unsigned)sym;
                    z->state = S_LENS_REPEAT;
                    break;
                }
                if (!going || z->state != S_LENS) break;
                {
                    /* A block must be able to end. */
                    if (z->lengths[256] == 0) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                    int left = construct(&z->lencode, z->lcount, z->lsymbol, z->lfast, LFAST_BITS, z->lengths, (int)z->nlen);
                    if (!usable(&z->lencode, left)) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                    left = construct(&z->distcode, z->dcount, z->dsymbol, z->dfast, DFAST_BITS, z->lengths + z->nlen, (int)z->ndist);
                    if (!usable(&z->distcode, left)) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                    z->lc = &z->lencode; z->dc = &z->distcode;
                    z->state = S_SYMBOL;
                }
                break;
            case S_LENS_REPEAT: {
                /* 16: the previous length 3 to 6 times. 17 and 18: zero, 3 to 10 or 11 to 138 times. */
                const unsigned bits = z->repeat_sym == 16 ? 2 : z->repeat_sym == 17 ? 3 : 7;
                if (!need(z, &io, bits)) { going = false; break; }
                unsigned times = take(z, bits) + (z->repeat_sym == 18 ? 11u : 3u);
                proven_u16 value = 0;
                if (z->repeat_sym == 16) {
                    if (z->have == 0) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                    value = z->lengths[z->have - 1];
                }
                if (z->have + times > z->nlen + z->ndist) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                while (times-- > 0) z->lengths[z->have++] = value;
                z->state = S_LENS;
                break; }
            case S_SYMBOL: {
                /* As many whole symbols as the tables find, when no symbol is half-read;
                 * then one the slow way, which is what can stop anywhere. */
                if (z->len == 1 && z->bitcnt < 8) {
                    fast(z, &io);
                    if (z->state != S_SYMBOL) break;
                }
                /* Decoded even when the output is full: the next symbol may be the end of the
                 * block, and a stream whose data fits exactly must be able to say it has ended. */
                const int sym = decode(z, &io, z->lc);
                if (sym == -1) { going = false; break; }
                if (sym < 0) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                if (sym < 256) {
                    if (io.out_at == io.out_len) { z->held = (proven_byte_t)sym; z->state = S_HELD; going = false; break; }
                    io.out[io.out_at++] = (proven_byte_t)sym;
                    break;
                }
                if (sym == 256) { z->state = S_BLOCK; break; }
                if (sym >= 257 + 29) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                z->length = LEN_BASE[sym - 257];
                z->extra = LEN_EXTRA[sym - 257];
                z->state = S_LEN_EXTRA;
                break; }
            case S_HELD:
                if (io.out_at == io.out_len) { going = false; break; }
                io.out[io.out_at++] = z->held;
                z->state = S_SYMBOL;
                break;
            case S_LEN_EXTRA:
                if (!need(z, &io, z->extra)) { going = false; break; }
                z->length += take(z, z->extra);
                z->state = S_DIST;
                break;
            case S_DIST: {
                const int sym = decode(z, &io, z->dc);
                if (sym == -1) { going = false; break; }
                if (sym < 0 || sym >= MAXDCODES) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                z->distance = DIST_BASE[sym];
                z->extra = DIST_EXTRA[sym];
                z->state = S_DIST_EXTRA;
                break; }
            case S_DIST_EXTRA:
                if (!need(z, &io, z->extra)) { going = false; break; }
                z->distance += take(z, z->extra);
                /* Further back than anything that was ever written. */
                if (z->distance > z->whave + io.out_at) { result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break; }
                z->state = S_COPY;
                break;
            case S_COPY:
                /* A byte at a time on purpose: when the distance is shorter than the length
                 * the copy reads what it has just written, and that is what it means. */
                while (z->length > 0) {
                    if (io.out_at == io.out_len) { going = false; break; }
                    io.out[io.out_at] = back(z, &io, z->distance);
                    io.out_at++;
                    z->length--;
                }
                if (z->length == 0) z->state = S_SYMBOL;
                break;
            case S_TRAILER: {
                /* zlib: the Adler-32 of the data, big-endian. gzip: its CRC-32 and its length
                 * modulo 2^32, little-endian. */
                const unsigned want = z->format == PROVEN_DEFLATE_ZLIB ? 4 : 8;
                while (z->have < want) {
                    if (io.in_at == io.in_len) { going = false; break; }
                    const proven_u32 b = io.in[io.in_at++];
                    if (z->format == PROVEN_DEFLATE_ZLIB) z->trailer[0] = (z->have == 0 ? 0 : z->trailer[0] << 8) | b;
                    else if (z->have < 4) z->trailer[0] = (z->have == 0 ? 0 : z->trailer[0]) | (b << (8 * z->have));
                    else z->trailer[1] = (z->have == 4 ? 0 : z->trailer[1]) | (b << (8 * (z->have - 4)));
                    z->have++;
                }
                if (!going) break;
                sum_output(z, &io);
                if (z->trailer[0] != z->check || (z->format == PROVEN_DEFLATE_GZIP && z->trailer[1] != (proven_u32)(z->total + io.out_at))) {
                    result = bad(z, PROVEN_ERR_INVALID_FORMAT); going = false; break;
                }
                z->state = S_DONE;
                break; }
            default:
                going = false;
                break;
        }
    }
    sum_output(z, &io);
    keep(z, &io);
    *consumed = io.in_at;
    *produced = io.out_at;
    *done = z->state == S_DONE;
    return result;
}

proven_err_t proven_inflate_all(proven_allocator_t alloc, proven_deflate_format_t format, proven_mem_view_t in,
                                proven_mem_mut_t out, proven_size_t *written, proven_size_t *consumed) {
    if (written) *written = 0;
    if (consumed) *consumed = 0;
    if (!written) return PROVEN_ERR_INVALID_ARG;
    proven_inflate_t *z = NULL;
    proven_err_t e = proven_inflate_create(alloc, format, &z);
    if (e != PROVEN_OK) return e;
    proven_size_t used = 0, made = 0;
    bool done = false;
    e = proven_inflate(z, in, &used, out, &made, &done);
    if (e == PROVEN_OK && !done && made == out.size) {
        /* The output is full and the stream has not ended. Either there is more data than the
         * caller allowed, or the data fitted exactly and only the end of the stream is still
         * to come: one more byte of room tells which. */
        proven_byte_t probe;
        proven_size_t more_in = 0, more_out = 0;
        e = proven_inflate(z, (proven_mem_view_t){ .ptr = in.ptr + used, .size = in.size - used }, &more_in, (proven_mem_mut_t){ .ptr = &probe, .size = 1 }, &more_out, &done);
        used += more_in;
        if (e == PROVEN_OK && more_out > 0) e = PROVEN_ERR_OUT_OF_BOUNDS;
    }
    /* Still not ended, with room to spare: the input stopped before the stream did. */
    if (e == PROVEN_OK && !done) e = PROVEN_ERR_INVALID_FORMAT;
    proven_inflate_destroy(z);
    if (e != PROVEN_OK) return e;
    *written = made;
    if (consumed) *consumed = used;
    return PROVEN_OK;
}

/* ================= Compression =================
 *
 * Input is gathered in a buffer twice the window's size. As each position is reached, the
 * three bytes there are hashed and chained to earlier positions with the same hash, and the
 * chain is followed for the longest earlier run that matches. What comes out of that - a
 * byte as it is, or "length, distance" - is kept as a token until a block's worth is there;
 * then the block is written in whichever of the three ways is shortest. The buffer slides by
 * a window when it is full, always at a block boundary, so the bytes of an unfinished block
 * are always still there to be stored as they are if that is the shortest way. */

#define MIN_MATCH 3
#define MAX_MATCH 258
#define MIN_LOOKAHEAD (MAX_MATCH + MIN_MATCH + 1)
#define MAX_TOKENS 16384u
#define NIL 0xffffu                     /* no position: the largest real one is two windows less three, 65533 */

typedef struct { proven_u16 key, sym; } proven_symfreq_t;

struct proven_deflate {
    proven_deflate_options_t opt;
    proven_size_t wsize;                /* the window: 1 << window_bits */
    proven_u32 hash_mask;
    unsigned max_chain, nice_len, lazy_len;
    proven_byte_t *buf;                 /* 2 * wsize bytes of input */
    proven_u16 *head, *prev;            /* hash table, and each position's predecessor in its chain */
    proven_u32 *tokens;                 /* literal: the byte. match: 0x80000000 | (length - 3) << 16 | (distance - 1) */
    proven_byte_t *pending;             /* output not yet taken */
    proven_size_t pend_cap, pend_len, pend_at;
    proven_size_t fill, pos, block_start, ntokens, max_tokens;
    bool have_prev;                     /* lazy matching: a match at pos - 1 is waiting to be judged */
    unsigned prev_len, prev_dist;
    proven_u64 bitbuf;
    unsigned bitcnt;
    proven_u32 check;
    proven_u64 total;
    bool header_done, finished, clean;  /* clean: nothing has been given since the last sync flush */
    /* per block */
    proven_u16 lfreq[288], dfreq[30], cfreq[19];
    proven_u16 llen[288], dlen[30], clen[19];
    proven_u16 lcode[288], dcode[30], ccode[19];
    proven_symfreq_t scratch[288];
    proven_u16 rle[288 + 30];           /* the code lengths as they are sent: symbols 0-18 and their extra bits */
    proven_u16 rle_extra[288 + 30];
};

static void put_bits(proven_deflate_t *z, proven_u32 value, unsigned n) {
    z->bitbuf |= (proven_u64)value << z->bitcnt;
    z->bitcnt += n;
    while (z->bitcnt >= 8) {
        z->pending[z->pend_len++] = (proven_byte_t)z->bitbuf;
        z->bitbuf >>= 8;
        z->bitcnt -= 8;
    }
}

static void put_align(proven_deflate_t *z) {
    if (z->bitcnt > 0) { z->pending[z->pend_len++] = (proven_byte_t)z->bitbuf; z->bitbuf = 0; z->bitcnt = 0; }
}

static void put_byte(proven_deflate_t *z, proven_u32 b) { z->pending[z->pend_len++] = (proven_byte_t)b; }

/* A Huffman code goes out most significant bit first. */
static proven_u16 reverse_bits(proven_u32 code, unsigned len) {
    proven_u32 r = 0;
    for (unsigned i = 0; i < len; ++i) { r = (r << 1) | (code & 1u); code >>= 1; }
    return (proven_u16)r;
}

/* Code lengths of minimum redundancy for frequencies sorted in rising order (Moffat and
 * Katajainen, in place): on return each key is that symbol's code length. */
static void minimum_redundancy(proven_symfreq_t *a, int n) {
    if (n == 0) return;
    if (n == 1) { a[0].key = 1; return; }
    int root = 0, leaf = 2, next;
    a[0].key = (proven_u16)(a[0].key + a[1].key);
    for (next = 1; next < n - 1; ++next) {
        if (leaf >= n || a[root].key < a[leaf].key) { a[next].key = a[root].key; a[root++].key = (proven_u16)next; }
        else a[next].key = a[leaf++].key;
        if (leaf >= n || (root < next && a[root].key < a[leaf].key)) { a[next].key = (proven_u16)(a[next].key + a[root].key); a[root++].key = (proven_u16)next; }
        else a[next].key = (proven_u16)(a[next].key + a[leaf++].key);
    }
    a[n - 2].key = 0;
    for (next = n - 3; next >= 0; --next) a[next].key = (proven_u16)(a[a[next].key].key + 1);
    int avail = 1, used = 0, depth = 0;
    root = n - 2; next = n - 1;
    while (avail > 0) {
        while (root >= 0 && (int)a[root].key == depth) { used++; root--; }
        while (avail > used) { a[next--].key = (proven_u16)depth; avail--; }
        avail = 2 * used; depth++; used = 0;
    }
}

/* From frequencies to a complete code no longer than `limit` bits: lengths and codes for the
 * `n` symbols. At least two symbols are given a code, so the code is never the one-symbol
 * special case that some decoders stumble on. Frequencies are scaled down first if their sum
 * would not fit the sixteen bits the tree is built in. */
static void make_code(proven_deflate_t *z, const proven_u16 *freq, int n, int limit, proven_u16 *len, proven_u16 *code) {
    proven_symfreq_t *a = z->scratch;
    int used = 0, counts[MAXBITS + 2] = { 0 };
    proven_u32 sum = 0;
    for (int i = 0; i < n; ++i) sum += freq[i];
    const unsigned shift = sum > 60000u ? 1u : 0u;                 /* a block holds at most 16384 tokens + 1: one halving is enough */
    for (int i = 0; i < n; ++i) { len[i] = 0; code[i] = 0; if (freq[i]) { a[used].key = (proven_u16)((freq[i] >> shift) | 1u); a[used].sym = (proven_u16)i; used++; } }
    for (int i = 0; used < 2 && i < n; ++i) if (!freq[i]) { a[used].key = 1; a[used].sym = (proven_u16)i; used++; }
    /* Sort by frequency, rising (insertion sort: the alphabets are small). */
    for (int i = 1; i < used; ++i) {
        const proven_symfreq_t v = a[i];
        int j = i;
        while (j > 0 && (a[j - 1].key > v.key || (a[j - 1].key == v.key && a[j - 1].sym > v.sym))) { a[j] = a[j - 1]; --j; }
        a[j] = v;
    }
    minimum_redundancy(a, used);
    for (int i = 0; i < used; ++i) counts[a[i].key > (proven_u16)limit ? limit : a[i].key]++;
    /* Too-long codes were just counted as `limit` bits, which over-fills the code space: move
     * codes down a level until it is exactly full again. */
    proven_u32 total = 0;
    for (int i = limit; i > 0; --i) total += (proven_u32)counts[i] << (limit - i);
    while (total != (1u << limit)) {
        counts[limit]--;
        for (int i = limit - 1; i > 0; --i) if (counts[i]) { counts[i]--; counts[i + 1] += 2; break; }
        total--;
    }
    /* The rarest symbols get the longest codes. */
    int at = 0;
    for (int bits = limit; bits > 0; --bits) for (int k = 0; k < counts[bits]; ++k) len[a[at++].sym] = (proven_u16)bits;
    /* Canonical codes: by length, then by symbol. */
    proven_u32 next_code[MAXBITS + 2], c = 0;
    next_code[0] = 0;
    for (int bits = 1; bits <= limit; ++bits) { c = (c + (proven_u32)(bits > 1 ? counts[bits - 1] : 0)) << 1; next_code[bits] = c; }
    for (int i = 0; i < n; ++i) if (len[i]) code[i] = reverse_bits(next_code[len[i]]++, len[i]);
}

static unsigned length_symbol(unsigned length, unsigned *extra_bits, unsigned *extra) {
    unsigned s = 28;
    while (s > 0 && LEN_BASE[s] > length) --s;
    *extra_bits = LEN_EXTRA[s];
    *extra = length - LEN_BASE[s];
    return 257 + s;
}

static unsigned distance_symbol(unsigned distance, unsigned *extra_bits, unsigned *extra) {
    unsigned lo = 0, hi = 29;
    while (lo < hi) { const unsigned mid = (lo + hi + 1) / 2; if (DIST_BASE[mid] <= distance) lo = mid; else hi = mid - 1; }
    *extra_bits = DIST_EXTRA[lo];
    *extra = distance - DIST_BASE[lo];
    return lo;
}

/* The tokens of the block, with the given codes. */
static void put_tokens(proven_deflate_t *z, const proven_u16 *llen, const proven_u16 *lcode, const proven_u16 *dlen, const proven_u16 *dcode) {
    for (proven_size_t i = 0; i < z->ntokens; ++i) {
        const proven_u32 t = z->tokens[i];
        if (!(t & 0x80000000u)) { put_bits(z, lcode[t], llen[t]); continue; }
        unsigned eb, ev;
        unsigned s = length_symbol(((t >> 16) & 0xff) + MIN_MATCH, &eb, &ev);
        put_bits(z, lcode[s], llen[s]);
        if (eb) put_bits(z, ev, eb);
        s = distance_symbol((t & 0xffff) + 1, &eb, &ev);
        put_bits(z, dcode[s], dlen[s]);
        if (eb) put_bits(z, ev, eb);
    }
    put_bits(z, lcode[256], llen[256]);
}

/* Write the tokens gathered so far as one block, the shortest way, and forget them. */
static void emit_block(proven_deflate_t *z, bool last) {
    const proven_size_t span = z->pos - z->block_start;             /* the input bytes the tokens stand for */
    static const proven_u16 FIXED_LLEN_RANGES[4] = { 144, 256, 280, 288 };
    proven_u16 fl[288], fc[288], fdl[30], fdc[30];
    /* Frequencies. */
    for (int i = 0; i < 288; ++i) z->lfreq[i] = 0;
    for (int i = 0; i < 30; ++i) z->dfreq[i] = 0;
    for (proven_size_t i = 0; i < z->ntokens; ++i) {
        const proven_u32 t = z->tokens[i];
        unsigned eb, ev;
        if (!(t & 0x80000000u)) { z->lfreq[t]++; continue; }
        z->lfreq[length_symbol(((t >> 16) & 0xff) + MIN_MATCH, &eb, &ev)]++;
        z->dfreq[distance_symbol((t & 0xffff) + 1, &eb, &ev)]++;
    }
    z->lfreq[256] = 1;
    /* The fixed code, and what the block costs in it. */
    {
        int sym = 0;
        proven_u32 next8 = 0x30, next9 = 0x190, next7 = 0, next8b = 0xc0;
        for (; sym < FIXED_LLEN_RANGES[0]; ++sym) { fl[sym] = 8; fc[sym] = reverse_bits(next8++, 8); }
        for (; sym < FIXED_LLEN_RANGES[1]; ++sym) { fl[sym] = 9; fc[sym] = reverse_bits(next9++, 9); }
        for (; sym < FIXED_LLEN_RANGES[2]; ++sym) { fl[sym] = 7; fc[sym] = reverse_bits(next7++, 7); }
        for (; sym < FIXED_LLEN_RANGES[3]; ++sym) { fl[sym] = 8; fc[sym] = reverse_bits(next8b++, 8); }
        for (sym = 0; sym < 30; ++sym) { fdl[sym] = 5; fdc[sym] = reverse_bits((proven_u32)sym, 5); }
    }
    proven_u64 extra_bits = 0, fixed_bits = 3, dynamic_bits = 3;
    for (int i = 0; i < 29; ++i) extra_bits += (proven_u64)z->lfreq[257 + i] * LEN_EXTRA[i];
    for (int i = 0; i < 30; ++i) extra_bits += (proven_u64)z->dfreq[i] * DIST_EXTRA[i];
    for (int i = 0; i < 288; ++i) fixed_bits += (proven_u64)z->lfreq[i] * fl[i];
    for (int i = 0; i < 30; ++i) fixed_bits += (proven_u64)z->dfreq[i] * 5;
    fixed_bits += extra_bits;
    /* The block's own code, and its cost with the header that describes it. */
    make_code(z, z->lfreq, 286, MAXBITS, z->llen, z->lcode);
    make_code(z, z->dfreq, 30, MAXBITS, z->dlen, z->dcode);
    int nlen = 286, ndist = 30;
    while (nlen > 257 && z->llen[nlen - 1] == 0) --nlen;
    while (ndist > 1 && z->dlen[ndist - 1] == 0) --ndist;
    /* The lengths, run-length coded with the three repeat symbols. */
    int nrle = 0;
    {
        proven_u16 all[286 + 30];
        const int total = nlen + ndist;
        for (int i = 0; i < nlen; ++i) all[i] = z->llen[i];
        for (int i = 0; i < ndist; ++i) all[nlen + i] = z->dlen[i];
        for (int i = 0; i < 19; ++i) z->cfreq[i] = 0;
        for (int i = 0; i < total;) {
            int run = 1;
            while (i + run < total && all[i + run] == all[i]) ++run;
            if (all[i] == 0 && run >= 3) {
                const int take_ = run > 138 ? 138 : run;
                z->rle[nrle] = (proven_u16)(take_ <= 10 ? 17 : 18);
                z->rle_extra[nrle++] = (proven_u16)(take_ <= 10 ? take_ - 3 : take_ - 11);
                i += take_;
            } else if (all[i] != 0 && run >= 4) {
                /* The length itself once, then "repeat the previous" for up to six more. */
                z->rle[nrle] = all[i]; z->rle_extra[nrle++] = 0;
                int left = run - 1;
                i += 1;
                while (left >= 3) {
                    const int take_ = left > 6 ? 6 : left;
                    z->rle[nrle] = 16; z->rle_extra[nrle++] = (proven_u16)(take_ - 3);
                    left -= take_; i += take_;
                }
            } else {
                z->rle[nrle] = all[i]; z->rle_extra[nrle++] = 0;
                i += 1;
            }
        }
        for (int i = 0; i < nrle; ++i) z->cfreq[z->rle[i]]++;
    }
    make_code(z, z->cfreq, 19, 7, z->clen, z->ccode);
    int ncode = 19;
    while (ncode > 4 && z->clen[CODE_ORDER[ncode - 1]] == 0) --ncode;
    dynamic_bits += 14 + 3 * (proven_u64)ncode;
    for (int i = 0; i < nrle; ++i) dynamic_bits += z->clen[z->rle[i]] + (z->rle[i] == 16 ? 2u : z->rle[i] == 17 ? 3u : z->rle[i] == 18 ? 7u : 0u);
    for (int i = 0; i < 286; ++i) dynamic_bits += (proven_u64)z->lfreq[i] * z->llen[i];
    for (int i = 0; i < 30; ++i) dynamic_bits += (proven_u64)z->dfreq[i] * z->dlen[i];
    dynamic_bits += extra_bits;
    /* Stored: the bytes as they are, in pieces of at most 65535, each with five bytes in front
     * (after the bits that are left over are filled out to a byte). */
    const proven_u64 pieces = span == 0 ? 1 : (span + 65534) / 65535;
    const proven_u64 stored_bits = pieces * 40 + (proven_u64)span * 8 + 7;
    const bool no_compress = z->opt.level < 0;

    if (no_compress || (stored_bits <= fixed_bits && stored_bits <= dynamic_bits)) {
        proven_size_t at = z->block_start, left = span;
        do {
            const proven_size_t n = left > 65535 ? 65535 : left;
            put_bits(z, last && n == left ? 1u : 0u, 1);
            put_bits(z, 0, 2);
            put_align(z);
            put_byte(z, (proven_u32)n & 0xff); put_byte(z, (proven_u32)n >> 8);
            put_byte(z, ~(proven_u32)n & 0xff); put_byte(z, (~(proven_u32)n >> 8) & 0xff);
            for (proven_size_t i = 0; i < n; ++i) z->pending[z->pend_len++] = z->buf[at + i];
            at += n; left -= n;
        } while (left > 0);
    } else if (fixed_bits <= dynamic_bits) {
        put_bits(z, last ? 1u : 0u, 1);
        put_bits(z, 1, 2);
        put_tokens(z, fl, fc, fdl, fdc);
    } else {
        put_bits(z, last ? 1u : 0u, 1);
        put_bits(z, 2, 2);
        put_bits(z, (proven_u32)(nlen - 257), 5);
        put_bits(z, (proven_u32)(ndist - 1), 5);
        put_bits(z, (proven_u32)(ncode - 4), 4);
        for (int i = 0; i < ncode; ++i) put_bits(z, z->clen[CODE_ORDER[i]], 3);
        for (int i = 0; i < nrle; ++i) {
            put_bits(z, z->ccode[z->rle[i]], z->clen[z->rle[i]]);
            if (z->rle[i] == 16) put_bits(z, z->rle_extra[i], 2);
            else if (z->rle[i] == 17) put_bits(z, z->rle_extra[i], 3);
            else if (z->rle[i] == 18) put_bits(z, z->rle_extra[i], 7);
        }
        put_tokens(z, z->llen, z->lcode, z->dlen, z->dcode);
    }
    z->ntokens = 0;
    z->block_start = z->pos;
}

static proven_u32 hash3(const proven_deflate_t *z, const proven_byte_t *p) {
    return ((((proven_u32)p[0] << 10) ^ ((proven_u32)p[1] << 5) ^ (proven_u32)p[2]) * 0x9e3779b1u >> 16) & z->hash_mask;
}

static void insert(proven_deflate_t *z, proven_size_t at) {
    const proven_u32 h = hash3(z, z->buf + at);
    z->prev[at & (z->wsize - 1)] = z->head[h];
    z->head[h] = (proven_u16)at;
}

/* The longest match for the bytes at `at` among the earlier positions with the same hash,
 * looking no further back than the window allows and no longer than `limit` bytes. */
static unsigned longest_match(const proven_deflate_t *z, proven_size_t at, unsigned limit, unsigned *distance) {
    const proven_byte_t *here = z->buf + at;
    const proven_size_t reach = z->wsize - MIN_LOOKAHEAD;           /* as far back as is always still in the buffer */
    const proven_size_t floor_ = at > reach ? at - reach : 0;
    unsigned best = MIN_MATCH - 1, chain = z->max_chain;
    proven_size_t cand = z->head[hash3(z, here)];
    while (cand != NIL && cand >= floor_ && cand < at && chain-- > 0) {
        const proven_byte_t *there = z->buf + cand;
        if (there[best] == here[best] && there[0] == here[0]) {
            unsigned n = 0;
            while (n < limit && there[n] == here[n]) ++n;
            if (n > best) { best = n; *distance = (unsigned)(at - cand); if (n >= z->nice_len || n >= limit) break; }
        }
        const proven_size_t before = z->prev[cand & (z->wsize - 1)];
        if (before == NIL || before >= cand) break;                 /* a chain only ever goes back */
        cand = before;
    }
    return best >= MIN_MATCH ? best : 0;
}

/* Make room: the upper half of the buffer becomes the lower, and every remembered position
 * moves with it. Only at a block boundary. */
static void slide(proven_deflate_t *z) {
    const proven_size_t w = z->wsize;
    for (proven_size_t i = 0; i < w; ++i) z->buf[i] = z->buf[i + w];
    z->fill -= w; z->pos -= w; z->block_start -= w;
    for (proven_size_t i = 0; i <= z->hash_mask; ++i) z->head[i] = (proven_u16)(z->head[i] != NIL && z->head[i] >= w ? z->head[i] - w : NIL);
    for (proven_size_t i = 0; i < w; ++i) z->prev[i] = (proven_u16)(z->prev[i] != NIL && z->prev[i] >= w ? z->prev[i] - w : NIL);
}

static void token_literal(proven_deflate_t *z, proven_byte_t b) { z->tokens[z->ntokens++] = b; }
static void token_match(proven_deflate_t *z, unsigned length, unsigned distance) {
    z->tokens[z->ntokens++] = 0x80000000u | ((proven_u32)(length - MIN_MATCH) << 16) | (proven_u32)(distance - 1);
}

static void deflate_start(proven_deflate_t *z) {
    z->pend_len = 0; z->pend_at = 0;
    z->fill = 0; z->pos = 0; z->block_start = 0; z->ntokens = 0;
    z->have_prev = false; z->prev_len = 0; z->prev_dist = 0;
    z->bitbuf = 0; z->bitcnt = 0;
    z->check = z->opt.format == PROVEN_DEFLATE_ZLIB ? 1u : 0u;
    z->total = 0;
    z->header_done = false; z->finished = false; z->clean = true;
    for (proven_size_t i = 0; i <= z->hash_mask; ++i) z->head[i] = NIL;
    for (proven_size_t i = 0; i < z->wsize; ++i) z->prev[i] = NIL;
}

proven_err_t proven_deflate_create(const proven_deflate_options_t *o, proven_deflate_t **out) {
    static const struct { proven_u16 chain, nice, lazy; } LEVELS[10] = {
        { 0, 0, 0 }, { 4, 8, 0 }, { 6, 16, 0 }, { 12, 32, 0 }, { 16, 32, 8 }, { 32, 64, 16 },
        { 128, 128, 32 }, { 256, 128, 64 }, { 1024, 258, 128 }, { 4096, 258, 258 },
    };
    if (!out) return PROVEN_ERR_INVALID_ARG;
    *out = NULL;
    if (!o || !proven_alloc_is_valid(o->alloc) || (unsigned)o->format > (unsigned)PROVEN_DEFLATE_GZIP || o->level < -1 || o->level > 9 ||
        (o->window_bits != 0 && (o->window_bits < 9 || o->window_bits > 15))) return PROVEN_ERR_INVALID_ARG;
    const unsigned bits = o->window_bits ? (unsigned)o->window_bits : 15u;
    const proven_size_t w = (proven_size_t)1 << bits;
    const proven_size_t max_tokens = w < MAX_TOKENS ? w : MAX_TOKENS;
    /* One block is never more than the buffer, and stored is the longest way to write it. */
    const proven_size_t pend_cap = 2 * w + 5 * ((2 * w) / 65535 + 2) + 64;
    /* One allocation: the struct, then the arrays in order of alignment. */
    const proven_size_t at_tokens = (sizeof(proven_deflate_t) + 15) & ~(proven_size_t)15;
    const proven_size_t at_head = at_tokens + max_tokens * sizeof(proven_u32);
    const proven_size_t at_prev = at_head + w * sizeof(proven_u16);
    const proven_size_t at_buf = at_prev + w * sizeof(proven_u16);
    const proven_size_t at_pend = at_buf + 2 * w;
    proven_result_mem_mut_t m = o->alloc.alloc_fn(o->alloc.ctx, at_pend + pend_cap, 16);
    if (m.err != PROVEN_OK) return PROVEN_ERR_NOMEM;
    proven_deflate_t *z = (proven_deflate_t *)(void *)m.value.ptr;
    z->opt = *o;
    z->opt.window_bits = (int)bits;
    if (z->opt.level == 0) z->opt.level = 6;
    z->wsize = w;
    z->hash_mask = (proven_u32)w - 1;
    z->max_tokens = max_tokens;
    const int lv = z->opt.level < 0 ? 0 : z->opt.level;
    z->max_chain = LEVELS[lv].chain; z->nice_len = LEVELS[lv].nice; z->lazy_len = LEVELS[lv].lazy;
    z->tokens = (proven_u32 *)(void *)(m.value.ptr + at_tokens);
    z->head = (proven_u16 *)(void *)(m.value.ptr + at_head);
    z->prev = (proven_u16 *)(void *)(m.value.ptr + at_prev);
    z->buf = m.value.ptr + at_buf;
    z->pending = m.value.ptr + at_pend;
    z->pend_cap = pend_cap;
    deflate_start(z);
    *out = z;
    return PROVEN_OK;
}

void proven_deflate_destroy(proven_deflate_t *z) {
    if (z) z->opt.alloc.free_fn(z->opt.alloc.ctx, z);
}

void proven_deflate_reset(proven_deflate_t *z) {
    if (z) deflate_start(z);
}

/* Turn buffered input into tokens for as long as there is enough ahead to judge a match -
 * or, when `all` says no more is coming for now, to the end. Stops when the block is full. */
static void tokenise(proven_deflate_t *z, bool all) {
    while (z->ntokens + 2 <= z->max_tokens) {
        const proven_size_t ahead = z->fill - z->pos;
        if (ahead == 0 || (!all && ahead < MIN_LOOKAHEAD)) break;
        if (z->opt.level < 0) { token_literal(z, z->buf[z->pos++]); continue; }
        unsigned dist = 0, len = 0;
        const unsigned limit = ahead < MAX_MATCH ? (unsigned)ahead : MAX_MATCH;
        if (ahead >= MIN_MATCH) {
            len = longest_match(z, z->pos, limit, &dist);
            insert(z, z->pos);
        }
        if (z->lazy_len == 0) {
            /* Greedy: take what was found. */
            if (len >= MIN_MATCH) {
                token_match(z, len, dist);
                for (unsigned k = 1; k < len; ++k) if (z->fill - (z->pos + k) >= MIN_MATCH) insert(z, z->pos + k);
                z->pos += len;
            } else {
                token_literal(z, z->buf[z->pos++]);
            }
            continue;
        }
        /* Lazy: a match found at the previous position is only used if this position does
         * not offer a longer one; otherwise that byte goes out alone and this match waits. */
        if (z->have_prev) {
            if (len > z->prev_len) {
                token_literal(z, z->buf[z->pos - 1]);
                z->prev_len = len; z->prev_dist = dist;
                z->pos++;
            } else {
                token_match(z, z->prev_len, z->prev_dist);
                /* The match began one byte back and covers prev_len bytes: pos - 1 .. pos + prev_len - 2. */
                const proven_size_t end = z->pos - 1 + z->prev_len;
                for (proven_size_t k = z->pos + 1; k < end; ++k) if (z->fill - k >= MIN_MATCH) insert(z, k);
                z->pos = end;
                z->have_prev = false;
            }
        } else if (len >= MIN_MATCH && len < z->lazy_len && ahead > len) {
            z->have_prev = true; z->prev_len = len; z->prev_dist = dist;
            z->pos++;
        } else if (len >= MIN_MATCH) {
            token_match(z, len, dist);
            for (unsigned k = 1; k < len; ++k) if (z->fill - (z->pos + k) >= MIN_MATCH) insert(z, z->pos + k);
            z->pos += len;
        } else {
            token_literal(z, z->buf[z->pos++]);
        }
    }
    /* Nothing more to look at: a waiting match is as good as it will get. */
    if (all && z->have_prev && z->fill == z->pos && z->ntokens + 1 <= z->max_tokens) {
        token_match(z, z->prev_len, z->prev_dist);
        z->pos = z->pos - 1 + z->prev_len;
        z->have_prev = false;
    }
}

proven_err_t proven_deflate(proven_deflate_t *z, proven_mem_view_t in, proven_size_t *consumed,
                            proven_mem_mut_t out, proven_size_t *produced, proven_deflate_flush_t flush, bool *done) {
    if (consumed) *consumed = 0;
    if (produced) *produced = 0;
    if (done) *done = false;
    if (!z || !consumed || !produced || !done || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr) ||
        (unsigned)flush > (unsigned)PROVEN_DEFLATE_FLUSH_FINISH) return PROVEN_ERR_INVALID_ARG;
    if (z->finished && in.size > 0) return PROVEN_ERR_INVALID_STATE;
    proven_size_t in_at = 0, out_at = 0;
    for (;;) {
        /* Hand over what is waiting. Nothing new is made while any of it is left. */
        while (z->pend_at < z->pend_len && out_at < out.size) out.ptr[out_at++] = z->pending[z->pend_at++];
        if (z->pend_at < z->pend_len) break;
        z->pend_len = 0; z->pend_at = 0;
        if (z->finished) break;
        if (!z->header_done) {
            if (z->opt.format == PROVEN_DEFLATE_ZLIB) {
                const proven_u32 cmf = 8u | ((proven_u32)(z->opt.window_bits - 8) << 4);
                const proven_u32 lv = z->opt.level < 2 ? 0u : z->opt.level < 6 ? 1u : z->opt.level == 6 ? 2u : 3u;
                proven_u32 flg = lv << 6;
                flg += 31 - ((cmf << 8) | flg) % 31;
                put_byte(z, cmf); put_byte(z, flg);
            } else if (z->opt.format == PROVEN_DEFLATE_GZIP) {
                static const proven_byte_t head[10] = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 255 };      /* no name, no time, system unknown */
                for (int i = 0; i < 10; ++i) put_byte(z, head[i]);
            }
            z->header_done = true;
            continue;
        }
        /* Take input into the buffer. When it is full, the block so far is written, the
         * buffer slides, and there is room again. */
        if (in_at < in.size && z->fill == 2 * z->wsize) {
            if (z->block_start < z->wsize || z->pos < z->wsize + (z->have_prev ? 1u : 0u)) {
                /* The block in progress reaches into the half that is about to go. End it. */
                tokenise(z, false);
                if (z->have_prev) { token_match(z, z->prev_len, z->prev_dist); z->pos = z->pos - 1 + z->prev_len; z->have_prev = false; }
                if (z->pos > z->block_start) { emit_block(z, false); continue; }
            }
            if (z->pos >= z->wsize && z->block_start >= z->wsize) slide(z);
        }
        proven_size_t room = 2 * z->wsize - z->fill, took = 0;
        while (room > 0 && in_at < in.size) { z->buf[z->fill++] = in.ptr[in_at++]; --room; ++took; }
        if (took > 0) {
            const proven_byte_t *p = in.ptr + in_at - took;
            if (z->opt.format == PROVEN_DEFLATE_ZLIB) z->check = adler32(z->check, p, took);
            else if (z->opt.format == PROVEN_DEFLATE_GZIP) z->check = proven_crc32_update(z->check, (proven_mem_view_t){ .ptr = p, .size = took });
            z->total += took;
            z->clean = false;
        }
        const bool all = flush != PROVEN_DEFLATE_FLUSH_NONE && in_at == in.size;
        tokenise(z, all);
        const bool block_full = z->ntokens + 2 > z->max_tokens;
        const bool drained = z->pos == z->fill && !z->have_prev;
        if (block_full) { emit_block(z, false); continue; }
        if (all && drained) {
            if (flush == PROVEN_DEFLATE_FLUSH_FINISH) {
                emit_block(z, true);
                put_align(z);
                if (z->opt.format == PROVEN_DEFLATE_ZLIB) { for (int i = 3; i >= 0; --i) put_byte(z, (z->check >> (8 * i)) & 0xff); }
                else if (z->opt.format == PROVEN_DEFLATE_GZIP) {
                    for (int i = 0; i < 4; ++i) put_byte(z, (z->check >> (8 * i)) & 0xff);
                    for (int i = 0; i < 4; ++i) put_byte(z, (proven_u32)(z->total >> (8 * i)) & 0xff);
                }
                z->finished = true;
                continue;
            }
            if (!z->clean) {
                /* Sync: what there is as a block, then an empty stored block, whose header
                 * pads to a byte boundary and whose body is 00 00 ff ff. */
                if (z->pos > z->block_start) emit_block(z, false);
                put_bits(z, 0, 3);
                put_align(z);
                put_byte(z, 0); put_byte(z, 0); put_byte(z, 0xff); put_byte(z, 0xff);
                z->clean = true;
                continue;
            }
        }
        /* The buffer is full, nothing could be turned into tokens and there is still input:
         * go round again to make room. Otherwise this call has done what it can. */
        if (in_at < in.size && z->fill == 2 * z->wsize) continue;
        break;
    }
    *consumed = in_at;
    *produced = out_at;
    *done = z->finished && z->pend_at == z->pend_len;
    return PROVEN_OK;
}

proven_size_t proven_deflate_bound(proven_deflate_format_t format, proven_size_t input_size) {
    /* Stored blocks are the worst case: five bytes for each, and a block is never longer than
     * the smallest buffer (two windows of 512 bytes). Then the framing. */
    const proven_size_t blocks = input_size / 512 + 2;
    return input_size + 5 * blocks + (format == PROVEN_DEFLATE_GZIP ? 18u : format == PROVEN_DEFLATE_ZLIB ? 6u : 0u) + 8;
}

proven_err_t proven_deflate_all(const proven_deflate_options_t *options, proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    if (written) *written = 0;
    if (!written) return PROVEN_ERR_INVALID_ARG;
    proven_deflate_t *z = NULL;
    proven_err_t e = proven_deflate_create(options, &z);
    if (e != PROVEN_OK) return e;
    proven_size_t used = 0, made = 0, in_at = 0, out_at = 0;
    bool done = false;
    while (e == PROVEN_OK && !done) {
        /* An empty input may be {NULL, 0}: no offset is applied to it (NULL + 0 is undefined). */
        e = proven_deflate(z, (proven_mem_view_t){ .ptr = in_at ? in.ptr + in_at : in.ptr, .size = in.size - in_at }, &used,
                           (proven_mem_mut_t){ .ptr = out_at ? out.ptr + out_at : out.ptr, .size = out.size - out_at }, &made, PROVEN_DEFLATE_FLUSH_FINISH, &done);
        in_at += used; out_at += made;
        if (e == PROVEN_OK && !done && used == 0 && made == 0) e = PROVEN_ERR_OUT_OF_BOUNDS;
    }
    proven_deflate_destroy(z);
    if (e != PROVEN_OK) return e;
    *written = out_at;
    return PROVEN_OK;
}
