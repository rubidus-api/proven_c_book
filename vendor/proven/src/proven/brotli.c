#include "proven/brotli.h"

#ifndef PROVEN_NO_BROTLI

#include "brotli_dict.h"

/* Brotli (RFC 7932), decompression only.
 *
 * A state machine that can stop at any read: each item of the format - a field of a header, a
 * symbol with its extra bits, a block switch - is taken whole once all of its bits are there,
 * and left whole otherwise, so the bits that have arrived wait in the bit buffer for the next
 * call. Nothing is read further than the item in hand needs: a stream ends at a byte boundary
 * after its last item, and that is where `consumed` says it ended.
 *
 * Output goes into the history (`ring`), and from there to the caller's buffer. The history
 * grows by doubling as the output grows, until it is 2^WBITS bytes; from then on it is a ring,
 * and a byte goes in only where one the caller has taken was. The prefix codes of a meta-block
 * are built into one table (`tab`): a code is an offset into it, its first 256 entries indexed
 * by the next eight bits of input and a second-level table for each longer prefix. */

#define ROOT_BITS 8
#define DIST_CODES_MAX (16 + 120 + (48 << 3))   /* NDIRECT at most 15 << 3, NPOSTFIX at most 3 */
#define MAX_ALPHABET 704                /* the insert-and-copy code's; the others are smaller */
/* The most entries a meta-block's codes can need: for each category a block type code (at most
 * 632 entries) and a block count code (396), the two context maps' codes (646 each), and 256
 * literal codes (630 each), 256 insert-and-copy codes (1,080) and 256 distance codes (896).
 * Each figure is the largest table build() makes for its alphabet, found by the private check
 * brotli/table_bound.py and built here to confirm it. */
#define TAB_MAX (3u * (632u + 396u) + 2u * 646u + 256u * (630u + 1080u + 896u))
#define SLACK 32u                       /* bytes after the history that a copy may write past its end */
#define NO_SWITCH 0xffffffffu           /* the count of a category with one block type: a meta-block (at most 2^24 symbols) never counts it down to zero */

/* An entry of a code's table: symbol << 4 | length. In the first 256, a length above 8 is a
 * link instead: offset of the second-level table << 4 | 8 + its index bits; a second-level
 * entry's length is what the code has beyond the first eight bits. */
#define ENTRY(sym, len) ((proven_u16)(((unsigned)(sym) << 4) | (unsigned)(len)))

static const proven_u8 CL_LEN[16] = { 2, 2, 2, 3, 2, 2, 2, 4, 2, 2, 2, 3, 2, 2, 2, 4 };
static const proven_u8 CL_VAL[16] = { 0, 4, 3, 2, 0, 4, 3, 1, 0, 4, 3, 2, 0, 4, 3, 5 };
static const proven_u8 CL_ORDER[18] = { 1, 2, 3, 4, 0, 5, 17, 6, 16, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static const proven_u32 BC_BASE[26] = { 1, 5, 9, 13, 17, 25, 33, 41, 49, 65, 81, 97, 113, 145, 177, 209, 241, 305, 369, 497, 753, 1265, 2289, 4337, 8433, 16625 };
static const proven_u8 BC_EXTRA[26] = { 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8, 9, 10, 11, 12, 13, 24 };
static const proven_u32 INS_BASE[24] = { 0, 1, 2, 3, 4, 5, 6, 8, 10, 14, 18, 26, 34, 50, 66, 98, 130, 194, 322, 578, 1090, 2114, 6210, 22594 };
static const proven_u8 INS_EXTRA[24] = { 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9, 10, 12, 14, 24 };
static const proven_u32 COPY_BASE[24] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 18, 22, 30, 38, 54, 70, 102, 134, 198, 326, 582, 1094, 2118 };
static const proven_u8 COPY_EXTRA[24] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9, 10, 24 };
/* Section 5: the insert and copy length codes each 64 insert-and-copy codes from 128 begin at. */
static const proven_u8 CELL_INS[9] = { 0, 0, 8, 8, 0, 16, 8, 16, 16 };
static const proven_u8 CELL_COPY[9] = { 0, 8, 0, 8, 16, 0, 16, 8, 16 };

enum {
    S_WBITS, S_HEADER, S_METADATA, S_UNCOMPRESSED,
    S_NBLTYPES, S_BTYPE_CODE, S_BCOUNT_CODE, S_BCOUNT,
    S_DIST_PARAMS, S_CMODES, S_NTREES, S_RLEMAX, S_CMAP_CODE, S_CMAP, S_IMTF, S_TREES,
    S_COMMAND, S_COPY_EXTRA, S_LITERALS, S_DISTANCE, S_COPY, S_WORD, S_END, S_DONE
};

/* Reading a prefix code (section 3), which may stop between any two of its items. */
enum { PC_START, PC_LENGTHS_CODE, PC_LENGTHS, PC_BUILD };

typedef struct {
    proven_u32 n;                       /* block types */
    proven_u32 type, prev;              /* the current one and the one before */
    proven_u32 count;                   /* symbols left in the current block, or NO_SWITCH */
    proven_u32 type_code, count_code;   /* offsets into the table */
} proven_category_t;

struct proven_brotli_decoder {
    proven_allocator_t alloc;
    proven_u64 window_max;
    int state;
    proven_err_t error;
    proven_u64 bitbuf;                  /* bits not yet used, the next one lowest; zero above bitcnt */
    unsigned bitcnt;

    /* the stream */
    proven_u32 window;                  /* largest distance: 2^WBITS - 16 */
    proven_size_t full;                 /* the history's final size, 2^WBITS */
    proven_byte_t *ring;
    proven_size_t cap;                  /* allocated: a power of two, at most `full` */
    proven_u64 pos, flushed;            /* bytes decoded, bytes handed to the caller */
    proven_u32 dist[4];                 /* the last four distances, dist[(dist_at - 1) & 3] the last */
    proven_u32 dist_at;

    /* the meta-block */
    bool last;
    proven_u32 left;                    /* bytes of it still to come (or metadata to skip) */
    int cat;                            /* the category whose block types are being read: 0 literal, 1 command, 2 distance */
    proven_category_t c[3];
    proven_u32 npostfix, ndirect, dist_alphabet;
    /* Per distance code from 16 on, for the meta-block's NPOSTFIX and NDIRECT: its extra bits,
     * and the distance with extra bits of zero - the distance is base + (extra << NPOSTFIX). */
    proven_u8 dx_bits[DIST_CODES_MAX];
    proven_u32 dx_base[DIST_CODES_MAX];
    proven_u32 index;                   /* progress through a list: context modes, a map, codes */
    proven_u32 ntrees, rlemax;          /* the map being read */
    bool map_distance;                  /* which map that is */
    proven_u32 ntrees_l, ntrees_d;
    proven_u32 map_code;
    proven_u8 cmode[256];
    proven_u8 cmap_l[64 * 256];
    proven_u8 cmap_d[4 * 256];
    proven_u32 code_l[256], code_i[256], code_d[256];

    /* a command */
    proven_u32 insert, copy, copy_code;
    bool implicit_zero;
    proven_u32 distance;
    proven_byte_t word[64];             /* a dictionary word, transformed */
    proven_u32 word_len, word_at;

    /* a prefix code being read */
    int pc;
    proven_u32 pc_alphabet, pc_result;
    proven_u32 pc_space, pc_symbol, pc_prev_len, pc_repeat, pc_repeat_len, pc_nonzero;
    proven_u8 cl_len[18];
    proven_u8 lens[MAX_ALPHABET];
    proven_u16 cl_table[1u << ROOT_BITS];

    /* the codes of the meta-block */
    proven_u16 *tab;
    proven_size_t tab_len, tab_cap;
};

/* What one call works on. */
typedef struct {
    const proven_byte_t *in;
    proven_size_t in_len, in_at;
    proven_byte_t *out;
    proven_size_t out_len, out_at;
} proven_io_t;

static proven_err_t bad(proven_brotli_decoder_t *b, proven_err_t e) {
    b->error = e;
    return e;
}

/* --- bits --- */

static bool load(proven_brotli_decoder_t *b, proven_io_t *io) {
    if (io->in_at == io->in_len) return false;
    b->bitbuf |= (proven_u64)io->in[io->in_at++] << b->bitcnt;
    b->bitcnt += 8;
    return true;
}

/* At least `n` bits there (n at most 56). False when the input ran out first. */
static bool need(proven_brotli_decoder_t *b, proven_io_t *io, unsigned n) {
    while (b->bitcnt < n) if (!load(b, io)) return false;
    return true;
}

static proven_u32 peek(const proven_brotli_decoder_t *b, unsigned at, unsigned n) {
    return (proven_u32)((b->bitbuf >> at) & (((proven_u64)1 << n) - 1u));
}

static void drop(proven_brotli_decoder_t *b, unsigned n) {
    b->bitbuf >>= n;
    b->bitcnt -= n;
}

/* The symbol that `bits` (lowest first) begin with, and how many of them its code is. */
static inline unsigned lookup(const proven_u16 *t, proven_u64 bits, unsigned *len) {
    proven_u16 e = t[bits & 0xffu];
    unsigned l = e & 15u;
    if (l > ROOT_BITS) {
        e = t[(e >> 4) + (unsigned)((bits >> ROOT_BITS) & ((1u << (l - ROOT_BITS)) - 1u))];
        l = ROOT_BITS + (e & 15u);
    }
    *len = l;
    return e >> 4;
}

/* One symbol of the code at `t`, taken; false when its bits have not all arrived. */
static bool symbol(proven_brotli_decoder_t *b, proven_io_t *io, const proven_u16 *t, unsigned *sym) {
    for (;;) {
        unsigned len;
        const unsigned s = lookup(t, b->bitbuf, &len);
        if (len <= b->bitcnt) { drop(b, len); *sym = s; return true; }
        if (!load(b, io)) return false;
    }
}

/* --- prefix codes --- */

/* The low `len` bits of `code` (len at most 15) in reverse order, a byte at a time. */
static const proven_u8 REV8[256] = {
    0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
    8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
    4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
    12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
    2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
    10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
    6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
    14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
    1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
    9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
    5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
    13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
    3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
    11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
    7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
    15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255,
};
static unsigned reverse(unsigned code, unsigned len) {
    const unsigned r16 = ((unsigned)REV8[code & 0xffu] << 8) | REV8[(code >> 8) & 0xffu];
    return r16 >> (16 - len);
}

/* Build the table of a complete code from the lengths of `n` symbols into `t`, or with `t`
 * null only count its entries. Returns the number of entries. */
static proven_u32 build(proven_u16 *t, const proven_u8 *lens, unsigned n) {
    unsigned count[16] = { 0 }, left[16], offs[16];
    proven_u16 sorted[MAX_ALPHABET];
    for (unsigned s = 0; s < n; ++s) count[lens[s]]++;
    offs[1] = 0;
    for (unsigned l = 1; l < 15; ++l) offs[l + 1] = offs[l] + count[l];
    for (unsigned s = 0; s < n; ++s) if (lens[s]) sorted[offs[lens[s]]++] = (proven_u16)s;
    for (unsigned l = 0; l < 16; ++l) left[l] = count[l];
    proven_u32 size = 1u << ROOT_BITS;
    unsigned code = 0, at = 0, root = 0x100, sub = 0, sub_bits = 0;
    for (unsigned len = 1; len <= 15; ++len, code <<= 1) {
        for (unsigned k = 0; k < count[len]; ++k, ++code) {
            const unsigned sym = sorted[at++];
            const unsigned r = reverse(code, len);
            if (len <= ROOT_BITS) {
                if (t) for (unsigned i = r; i < (1u << ROOT_BITS); i += 1u << len) t[i] = ENTRY(sym, len);
            } else {
                if ((r & 0xffu) != root) {
                    /* A new second-level table, as large as the longest code under this prefix
                     * needs: the codes left of this length and longer fill it in order. */
                    root = r & 0xffu;
                    unsigned l = len;
                    sub_bits = len - ROOT_BITS;
                    int room = 1 << sub_bits;
                    while (l < 15) {
                        room -= (int)left[l];
                        if (room <= 0) break;
                        ++l; ++sub_bits;
                        room <<= 1;
                    }
                    sub = size;
                    size += 1u << sub_bits;
                    if (t) t[root] = ENTRY(sub, ROOT_BITS + sub_bits);
                }
                if (t) for (unsigned i = r >> ROOT_BITS; i < (1u << sub_bits); i += 1u << (len - ROOT_BITS)) t[sub + i] = ENTRY(sym, len - ROOT_BITS);
            }
            left[len]--;
        }
    }
    return size;
}

/* Room for `n` more entries in the table. */
static bool tab_room(proven_brotli_decoder_t *b, proven_size_t n) {
    if (b->tab_cap - b->tab_len >= n) return true;
    /* doubling, but never past what a meta-block can need */
    proven_size_t cap = b->tab_cap ? 2 * b->tab_cap : 4096;
    if (cap > TAB_MAX) cap = TAB_MAX;
    if (cap < b->tab_len + n) cap = b->tab_len + n;
    proven_result_mem_mut_t m = b->alloc.alloc_fn(b->alloc.ctx, cap * sizeof(proven_u16), 16);
    if (m.err != PROVEN_OK) return false;
    proven_u16 *t = (proven_u16 *)(void *)m.value.ptr;
    for (proven_size_t i = 0; i < b->tab_len; ++i) t[i] = b->tab[i];
    if (b->tab) b->alloc.free_fn(b->alloc.ctx, b->tab);
    b->tab = t;
    b->tab_cap = cap;
    return true;
}

/* The lengths in `lens` into the table; the code's offset in pc_result. */
static proven_err_t place(proven_brotli_decoder_t *b, unsigned n) {
    const proven_u32 size = build(NULL, b->lens, n);
    if (!tab_room(b, size)) return bad(b, PROVEN_ERR_NOMEM);
    b->pc_result = (proven_u32)b->tab_len;
    (void)build(b->tab + b->tab_len, b->lens, n);
    b->tab_len += size;
    return PROVEN_OK;
}

/* A code of one symbol: no bits, every entry that symbol. */
static proven_err_t place_one(proven_brotli_decoder_t *b, unsigned sym) {
    if (!tab_room(b, 1u << ROOT_BITS)) return bad(b, PROVEN_ERR_NOMEM);
    b->pc_result = (proven_u32)b->tab_len;
    for (unsigned i = 0; i < (1u << ROOT_BITS); ++i) b->tab[b->tab_len + i] = ENTRY(sym, 0);
    b->tab_len += 1u << ROOT_BITS;
    return PROVEN_OK;
}

static unsigned bits_for(unsigned alphabet) {
    unsigned n = 0;
    while ((1u << n) < alphabet) ++n;
    return n;
}

/* Read a prefix code over `pc_alphabet` symbols, from where the last call stopped. 1 when it
 * is in the table (pc_result), 0 when the input ran out, -1 on an error. */
static int prefix_code(proven_brotli_decoder_t *b, proven_io_t *io) {
    const unsigned alphabet = b->pc_alphabet;
    switch (b->pc) {
        case PC_START: {
            if (!need(b, io, 2)) return 0;
            const unsigned hskip = peek(b, 0, 2);
            if (hskip == 1) {
                /* Simple: up to four symbols, read whole. */
                if (!need(b, io, 4)) return 0;
                const unsigned nsym = peek(b, 2, 2) + 1, w = bits_for(alphabet);
                const unsigned total = 4 + nsym * w + (nsym == 4 ? 1u : 0u);
                if (!need(b, io, total)) return 0;
                unsigned s[4];
                for (unsigned i = 0; i < nsym; ++i) {
                    s[i] = peek(b, 4 + i * w, w);
                    if (s[i] >= alphabet) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
                    for (unsigned j = 0; j < i; ++j) if (s[j] == s[i]) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
                }
                const bool tree = nsym == 4 && peek(b, 4 + 4 * w, 1) != 0;
                drop(b, total);
                if (nsym == 1) return place_one(b, s[0]) == PROVEN_OK ? 1 : -1;
                for (unsigned i = 0; i < alphabet; ++i) b->lens[i] = 0;
                static const proven_u8 L[4][4] = { { 0 }, { 1, 1 }, { 1, 2, 2 }, { 2, 2, 2, 2 } };
                static const proven_u8 TREE1[4] = { 1, 2, 3, 3 };
                for (unsigned i = 0; i < nsym; ++i) b->lens[s[i]] = tree ? TREE1[i] : L[nsym - 1][i];
                return place(b, alphabet) == PROVEN_OK ? 1 : -1;
            }
            drop(b, 2);
            for (unsigned i = 0; i < 18; ++i) b->cl_len[i] = 0;
            b->pc_symbol = hskip;       /* where in CL_ORDER the lengths start */
            b->pc_space = 32;
            b->pc_nonzero = 0;
            b->pc = PC_LENGTHS_CODE;
        }
        /* fall through */
        case PC_LENGTHS_CODE:
            /* The code of the code lengths: until its space is used up, or all eighteen. */
            while (b->pc_symbol < 18) {
                unsigned len, v;
                for (;;) {
                    v = peek(b, 0, b->bitcnt < 4 ? b->bitcnt : 4);
                    len = CL_LEN[v];
                    if (len <= b->bitcnt) break;
                    if (!load(b, io)) return 0;
                }
                drop(b, len);
                const unsigned value = CL_VAL[v];
                b->cl_len[CL_ORDER[b->pc_symbol++]] = (proven_u8)value;
                if (value != 0) {
                    b->pc_nonzero++;
                    if ((32u >> value) > b->pc_space) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
                    b->pc_space -= 32u >> value;
                    if (b->pc_space == 0) break;
                }
            }
            if (!(b->pc_nonzero == 1 || b->pc_space == 0)) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
            if (b->pc_nonzero == 1) {
                unsigned only = 0;
                for (unsigned i = 0; i < 18; ++i) if (b->cl_len[i]) only = i;
                for (unsigned i = 0; i < (1u << ROOT_BITS); ++i) b->cl_table[i] = ENTRY(only, 0);
            } else {
                (void)build(b->cl_table, b->cl_len, 18);
            }
            for (unsigned i = 0; i < alphabet; ++i) b->lens[i] = 0;
            b->pc_symbol = 0;
            b->pc_space = 32768;
            b->pc_prev_len = 8;
            b->pc_repeat = 0;
            b->pc_repeat_len = 0;
            b->pc = PC_LENGTHS;
            /* fall through */
        case PC_LENGTHS:
            while (b->pc_symbol < alphabet && b->pc_space > 0) {
                /* A length, or a repeat with its extra bits: taken together. */
                unsigned len, extra = 0;
                unsigned s;
                for (;;) {
                    s = lookup(b->cl_table, b->bitbuf, &len);
                    extra = s == 16 ? 2 : s == 17 ? 3 : 0;
                    if (len + extra <= b->bitcnt) break;
                    if (!load(b, io)) return 0;
                }
                const unsigned x = peek(b, len, extra);
                drop(b, len + extra);
                if (s < 16) {
                    b->pc_repeat = 0;
                    if (s != 0) {
                        b->lens[b->pc_symbol] = (proven_u8)s;
                        b->pc_prev_len = s;
                        b->pc_space -= 32768u >> s;
                        if (b->pc_space > 32768) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
                    }
                    b->pc_symbol++;
                    continue;
                }
                /* 16 repeats the last nonzero length, 17 a zero; one after another of the
                 * same kind, they make one longer run. */
                const unsigned value = s == 16 ? b->pc_prev_len : 0;
                if (b->pc_repeat_len != value) { b->pc_repeat = 0; b->pc_repeat_len = value; }
                const unsigned old = b->pc_repeat;
                if (b->pc_repeat > 0) b->pc_repeat = (b->pc_repeat - 2) << extra;
                b->pc_repeat += x + 3;
                const unsigned add = b->pc_repeat - old;
                if (add > alphabet - b->pc_symbol) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
                if (value != 0) {
                    const proven_u64 used = (proven_u64)add * (32768u >> value);
                    if (used > b->pc_space) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
                    b->pc_space -= (proven_u32)used;
                    for (unsigned i = 0; i < add; ++i) b->lens[b->pc_symbol + i] = (proven_u8)value;
                }
                b->pc_symbol += add;
            }
            if (b->pc_space != 0) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
            return place(b, alphabet) == PROVEN_OK ? 1 : -1;
        default:
            break;
    }
    return -1;
}

/* Start reading a code over `alphabet` symbols. */
static void code_begin(proven_brotli_decoder_t *b, unsigned alphabet) {
    b->pc = PC_START;
    b->pc_alphabet = alphabet;
}

/* --- small fields --- */

/* NBLTYPES and NTREES (section 9.2): 1 to 256. */
static bool var_count(proven_brotli_decoder_t *b, proven_io_t *io, proven_u32 *v) {
    if (!need(b, io, 1)) return false;
    if (peek(b, 0, 1) == 0) { drop(b, 1); *v = 1; return true; }
    if (!need(b, io, 4)) return false;
    const unsigned n = peek(b, 1, 3);
    if (n == 0) { drop(b, 4); *v = 2; return true; }
    if (!need(b, io, 4 + n)) return false;
    *v = (1u << n) + peek(b, 4, n) + 1;
    drop(b, 4 + n);
    return true;
}

/* A block count: its code and extra bits, together. */
static bool block_count(proven_brotli_decoder_t *b, proven_io_t *io, const proven_u16 *t, proven_u32 *v) {
    for (;;) {
        unsigned len;
        const unsigned s = lookup(t, b->bitbuf, &len);
        if (len + BC_EXTRA[s] <= b->bitcnt) {
            *v = BC_BASE[s] + peek(b, len, BC_EXTRA[s]);
            drop(b, len + BC_EXTRA[s]);
            return true;
        }
        if (!load(b, io)) return false;
    }
}

/* A block switch: the type and the count, together (at most 15 + 15 + 24 bits). */
static bool block_switch(proven_brotli_decoder_t *b, proven_io_t *io, proven_category_t *c) {
    const proven_u16 *tt = b->tab + c->type_code, *ct = b->tab + c->count_code;
    for (;;) {
        unsigned l1, l2;
        const unsigned code = lookup(tt, b->bitbuf, &l1);
        const unsigned s = lookup(ct, b->bitbuf >> l1, &l2);
        const unsigned total = l1 + l2 + BC_EXTRA[s];
        if (total <= b->bitcnt) {
            c->count = BC_BASE[s] + peek(b, l1 + l2, BC_EXTRA[s]);
            drop(b, total);
            proven_u32 type = code == 0 ? c->prev : code == 1 ? c->type + 1 : code - 2;
            if (type >= c->n) type -= c->n;
            c->prev = c->type;
            c->type = type;
            return true;
        }
        if (!load(b, io)) return false;
    }
}

/* --- output --- */

/* Hand over what is decoded and not yet taken. */
static void flush(proven_brotli_decoder_t *b, proven_io_t *io) {
    while (b->flushed < b->pos && io->out_at < io->out_len) {
        const proven_size_t at = (proven_size_t)(b->flushed & (b->cap - 1));
        proven_size_t n = (proven_size_t)(b->pos - b->flushed);
        if (n > b->cap - at) n = b->cap - at;
        if (n > io->out_len - io->out_at) n = io->out_len - io->out_at;
        (void)proven_mem_copy(io->out + io->out_at, n, (proven_mem_view_t){ b->ring + at, n });
        io->out_at += n;
        b->flushed += n;
    }
}

/* Room in the history for `n` more bytes (n at most 64): grown while it is not yet full size,
 * else what the caller has not taken handed over. 1 there is, 0 there is not until the caller
 * takes more, -1 when memory ran out. */
static int room(proven_brotli_decoder_t *b, proven_io_t *io, proven_size_t n) {
    if (b->cap < b->full) {
        if (b->pos + n <= b->cap) return 1;
        proven_size_t cap = b->cap ? b->cap : 4096;
        while (cap < b->pos + n) cap *= 2;
        if (cap > b->full) cap = b->full;
        proven_result_mem_mut_t m = b->alloc.alloc_fn(b->alloc.ctx, cap + SLACK, 16);
        if (m.err != PROVEN_OK) { bad(b, PROVEN_ERR_NOMEM); return -1; }
        (void)proven_mem_copy(m.value.ptr, cap, (proven_mem_view_t){ b->ring, (proven_size_t)b->pos });
        if (b->ring) b->alloc.free_fn(b->alloc.ctx, b->ring);
        b->ring = m.value.ptr;
        b->cap = cap;
        if (b->pos + n <= b->cap) return 1;
    }
    if (b->pos + n - b->flushed <= b->cap) return 1;
    flush(b, io);
    return b->pos + n - b->flushed <= b->cap ? 1 : 0;
}

static void put(proven_brotli_decoder_t *b, proven_byte_t v) {
    b->ring[b->pos & (b->cap - 1)] = v;
    b->pos++;
}

static proven_byte_t back(const proven_brotli_decoder_t *b, proven_u64 distance) {
    return distance <= b->pos ? b->ring[(b->pos - distance) & (b->cap - 1)] : 0;
}

/* --- dictionary --- */

static unsigned ferment(proven_byte_t *w, unsigned len, unsigned at) {
    if (w[at] < 192) {
        if (w[at] >= 97 && w[at] <= 122) w[at] ^= 32;
        return 1;
    }
    if (w[at] < 224) {
        if (at + 1 < len) w[at + 1] ^= 32;
        return 2;
    }
    if (at + 2 < len) w[at + 2] ^= 5;
    return 3;
}

/* Section 8: word `index` of length `len`, changed by transform `id`, into b->word. */
static void dictionary_word(proven_brotli_decoder_t *b, unsigned len, proven_u32 index, unsigned id) {
    const proven_brotli_transform_t *t = &brotli_transforms[id];
    const unsigned char *w = brotli_words[len] + (proven_size_t)index * (len + 1);
    unsigned n = 0;
    for (unsigned i = 0; i < t->prefix_len; ++i) b->word[n++] = (proven_byte_t)t->prefix[i];
    const unsigned type = t->type;
    unsigned skip = 0, keep = len;
    if (type >= 3 && type <= 11) skip = type - 2 < len ? type - 2 : len;
    if (type >= 12) keep = type - 11 < len ? len - (type - 11) : 0;
    const unsigned start = n;
    for (unsigned i = skip; i < keep; ++i) b->word[n++] = w[i];
    const unsigned wl = n - start;
    if (type == 1 && wl > 0) (void)ferment(b->word + start, wl, 0);
    if (type == 2) for (unsigned i = 0; i < wl;) i += ferment(b->word + start, wl, i);
    for (unsigned i = 0; i < t->suffix_len; ++i) b->word[n++] = (proven_byte_t)t->suffix[i];
    b->word_len = n;
    b->word_at = 0;
}

/* --- the machine --- */

static void start(proven_brotli_decoder_t *b) {
    b->state = S_WBITS;
    b->error = PROVEN_OK;
    b->bitbuf = 0;
    b->bitcnt = 0;
    b->pos = 0;
    b->flushed = 0;
    b->dist[0] = 16; b->dist[1] = 15; b->dist[2] = 11; b->dist[3] = 4;
    b->dist_at = 4;
    b->full = 0;
}

proven_err_t proven_brotli_decoder_create(proven_allocator_t alloc, proven_u64 window_max, proven_brotli_decoder_t **out) {
    if (!out) return PROVEN_ERR_INVALID_ARG;
    *out = NULL;
    if (!proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
    if (window_max == 0) window_max = PROVEN_BROTLI_WINDOW_DEFAULT;
    if (window_max < 1024 || window_max > ((proven_u64)1 << 31)) return PROVEN_ERR_INVALID_ARG;
    proven_result_mem_mut_t m = alloc.alloc_fn(alloc.ctx, sizeof(proven_brotli_decoder_t), 16);
    if (m.err != PROVEN_OK) return PROVEN_ERR_NOMEM;
    proven_brotli_decoder_t *b = (proven_brotli_decoder_t *)(void *)m.value.ptr;
    b->alloc = alloc;
    b->window_max = window_max;
    b->ring = NULL;
    b->cap = 0;
    b->tab = NULL;
    b->tab_len = b->tab_cap = 0;
    start(b);
    *out = b;
    return PROVEN_OK;
}

void proven_brotli_decoder_destroy(proven_brotli_decoder_t *b) {
    if (!b) return;
    if (b->ring) b->alloc.free_fn(b->alloc.ctx, b->ring);
    if (b->tab) b->alloc.free_fn(b->alloc.ctx, b->tab);
    b->alloc.free_fn(b->alloc.ctx, b);
}

void proven_brotli_decoder_reset(proven_brotli_decoder_t *b) {
    if (b) start(b);
}

/* The window (section 9.1), checked against the limit before anything is allocated for it. */
static int stream_header(proven_brotli_decoder_t *b, proven_io_t *io) {
    if (!need(b, io, 1)) return 0;
    unsigned wbits, used;
    if (peek(b, 0, 1) == 0) { wbits = 16; used = 1; }
    else {
        if (!need(b, io, 4)) return 0;
        const unsigned n = peek(b, 1, 3);
        if (n != 0) { wbits = 17 + n; used = 4; }
        else {
            if (!need(b, io, 7)) return 0;
            const unsigned m = peek(b, 4, 3);
            if (m == 1) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }      /* the large-window variant */
            wbits = m == 0 ? 17 : 8 + m;
            used = 7;
        }
    }
    const proven_u32 window = (1u << wbits) - 16;
    if (window > b->window_max) { bad(b, PROVEN_ERR_OUT_OF_BOUNDS); return -1; }
    drop(b, used);
    b->window = window;
    b->full = (proven_size_t)1 << wbits;
    /* A history kept from the last stream that is larger than this one's is given back. */
    if (b->cap > b->full) {
        b->alloc.free_fn(b->alloc.ctx, b->ring);
        b->ring = NULL;
        b->cap = 0;
    }
    return 1;
}

/* The meta-block header up to the block types (section 9.2), read whole: at most 31 bits. */
static int meta_header(proven_brotli_decoder_t *b, proven_io_t *io) {
    if (!need(b, io, 1)) return 0;
    unsigned at = 0;
    const bool last = peek(b, at++, 1) != 0;
    if (last) {
        if (!need(b, io, 2)) return 0;
        if (peek(b, at++, 1)) { drop(b, at); b->state = S_END; return 1; }
    }
    if (!need(b, io, at + 2)) return 0;
    const unsigned mn = peek(b, at, 2);
    at += 2;
    if (mn == 3) {
        /* Metadata: skipped. */
        if (!need(b, io, at + 3)) return 0;
        if (peek(b, at, 1) != 0) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
        const unsigned nbytes = peek(b, at + 1, 2);
        at += 3;
        if (!need(b, io, at + 8 * nbytes)) return 0;
        proven_u32 skip = 0;
        if (nbytes > 0) {
            skip = peek(b, at, 8 * nbytes) + 1;
            if (nbytes > 1 && peek(b, at + 8 * (nbytes - 1), 8) == 0) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
        }
        drop(b, at + 8 * nbytes);
        if (peek(b, 0, b->bitcnt & 7u) != 0) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
        drop(b, b->bitcnt & 7u);
        b->last = last;
        b->left = skip;
        b->state = S_METADATA;
        return 1;
    }
    const unsigned nibbles = 4 + mn;
    if (!need(b, io, at + 4 * nibbles + (last ? 0u : 1u))) return 0;
    const proven_u32 mlen = peek(b, at, 4 * nibbles) + 1;
    if (nibbles > 4 && peek(b, at + 4 * (nibbles - 1), 4) == 0) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
    at += 4 * nibbles;
    const bool uncompressed = !last && peek(b, at++, 1) != 0;
    drop(b, at);
    b->last = last;
    b->left = mlen;
    if (uncompressed) {
        if (peek(b, 0, b->bitcnt & 7u) != 0) { bad(b, PROVEN_ERR_INVALID_FORMAT); return -1; }
        drop(b, b->bitcnt & 7u);
        b->state = S_UNCOMPRESSED;
        return 1;
    }
    b->tab_len = 0;
    b->cat = 0;
    b->state = S_NBLTYPES;
    return 1;
}

/* Whole bytes: from the bit buffer first (it holds whole bytes after a byte boundary). */
static bool next_byte(proven_brotli_decoder_t *b, proven_io_t *io, proven_byte_t *v) {
    if (b->bitcnt >= 8) { *v = (proven_byte_t)b->bitbuf; drop(b, 8); return true; }
    if (io->in_at == io->in_len) return false;
    *v = io->in[io->in_at++];
    return true;
}

/* The end of a meta-block: the next one, or the end of the stream. */
static void meta_end(proven_brotli_decoder_t *b) {
    b->state = b->last ? S_END : S_HEADER;
}

/* The literal context of section 7.1, from the last two bytes. */
static unsigned context(const proven_brotli_decoder_t *b, unsigned mode) {
    const unsigned p1 = back(b, 1), p2 = back(b, 2);
    switch (mode) {
        case 0: return p1 & 0x3fu;
        case 1: return p1 >> 2;
        case 2: return brotli_lut0[p1] | brotli_lut1[p2];
        default: return (unsigned)(brotli_lut2[p1] << 3) | brotli_lut2[p2];
    }
}

/* The distance codes from 16 on for the meta-block's NPOSTFIX and NDIRECT (section 4): their
 * extra bits, and their distance with extra bits of zero. Made once a meta-block. */
static void distance_codes(proven_brotli_decoder_t *b) {
    for (unsigned code = 16; code < b->dist_alphabet; ++code) {
        if (code < 16 + b->ndirect) { b->dx_bits[code] = 0; b->dx_base[code] = code - 15; continue; }
        const unsigned c = code - b->ndirect - 16;
        const unsigned nbits = 1 + (c >> (b->npostfix + 1));
        const proven_u32 offset = ((2u + ((c >> b->npostfix) & 1u)) << nbits) - 4;
        b->dx_bits[code] = (proven_u8)nbits;
        b->dx_base[code] = (offset << b->npostfix) + (c & ((1u << b->npostfix) - 1u)) + b->ndirect + 1;
    }
}

/* The distance of the command (section 4) from its code and extra bits; 0 if invalid. */
static inline proven_u32 distance_of(const proven_brotli_decoder_t *b, unsigned code, proven_u32 extra) {
    if (code < 16) {
        static const proven_u8 WHICH[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1 };
        static const proven_i8 DELTA[16] = { 0, 0, 0, 0, -1, 1, -2, 2, -3, 3, -1, 1, -2, 2, -3, 3 };
        const proven_i64 d = (proven_i64)b->dist[(b->dist_at - 1 - WHICH[code]) & 3u] + DELTA[code];
        return d > 0 ? (proven_u32)d : 0;
    }
    return b->dx_base[code] + (extra << b->npostfix);
}

static inline unsigned distance_extra(const proven_brotli_decoder_t *b, unsigned code) {
    return code < 16 ? 0u : b->dx_bits[code];
}

/* --- the fast path --- */

#if defined(__GNUC__) || defined(__clang__)
#define HOT static inline __attribute__((always_inline))
#else
#define HOT static inline
#endif

/* Eight bytes, lowest first; and stored so. Compilers make each one load or store. */
HOT proven_u64 get64(const proven_byte_t *p) {
    return (proven_u64)p[0] | ((proven_u64)p[1] << 8) | ((proven_u64)p[2] << 16) | ((proven_u64)p[3] << 24) |
           ((proven_u64)p[4] << 32) | ((proven_u64)p[5] << 40) | ((proven_u64)p[6] << 48) | ((proven_u64)p[7] << 56);
}
/* Bytes moved as they are, eight and sixteen at once: one load and one store each (a loop of
 * byte stores was left a loop at -O2). */
HOT void copy8(proven_byte_t *d, const proven_byte_t *s) { __builtin_memcpy(d, s, 8); }
HOT void copy16(proven_byte_t *d, const proven_byte_t *s) { __builtin_memcpy(d, s, 16); }

/* The bit reader of the fast path, in locals: the bits, how many, and where the input is. */
typedef struct {
    proven_u64 bits;
    unsigned n;
    const proven_byte_t *p;
} proven_brotli_reader_t;

/* At least 56 bits, from eight bytes of input that are there. Above `n` the bits are those of
 * the input bytes from `p` on - what the next fill ORs in again, unchanged - so they need not
 * be cleared here; sync_out clears them for the step-by-step path, which keeps zeros there. */
HOT void fill(proven_brotli_reader_t *r) {
    const unsigned k = (63u - r->n) >> 3;
    r->bits |= get64(r->p) << r->n;
    r->n += 8 * k;
    r->p += k;
}

HOT unsigned take_sym(proven_brotli_reader_t *r, const proven_u16 *t) {
    unsigned len;
    const unsigned s = lookup(t, r->bits, &len);
    r->bits >>= len;
    r->n -= len;
    return s;
}

HOT proven_u32 take_bits(proven_brotli_reader_t *r, unsigned n) {
    const proven_u32 v = (proven_u32)(r->bits & (((proven_u64)1 << n) - 1u));
    r->bits >>= n;
    r->n -= n;
    return v;
}

/* The reader into the decoder and the call's input, and back. */
static void sync_out(proven_brotli_decoder_t *b, proven_io_t *io, const proven_brotli_reader_t *r) {
    b->bitbuf = r->n < 64 ? r->bits & (((proven_u64)1 << r->n) - 1u) : r->bits;
    b->bitcnt = r->n;
    io->in_at = (proven_size_t)(r->p - io->in);
}
static void sync_in(const proven_brotli_decoder_t *b, const proven_io_t *io, proven_brotli_reader_t *r) {
    r->bits = b->bitbuf;
    r->n = b->bitcnt;
    r->p = io->in + io->in_at;
}

/* A copy inside the history of `n` bytes from `d` back, which have room; the new position.
 * `spare`: the 64 bytes after the copy have room too, so whole blocks of sixteen may be
 * written past its end (into them, or into the SLACK after the history's last byte). */
static proven_u64 copy_back(proven_byte_t *ring, proven_size_t cap, proven_u64 pos, proven_u64 d, proven_u32 n, bool spare) {
    const proven_size_t mask = cap - 1;
    if (spare) {
        const proven_size_t to = (proven_size_t)(pos & mask), from = (proven_size_t)((pos - d) & mask);
        if (to + n <= cap && from + n <= cap) {
            proven_byte_t *dst = ring + to;
            const proven_byte_t *src = ring + from;
            if (d >= 16) {
                /* Sixteen at a time: each sixteen read were written at least sixteen bytes before. */
                for (proven_size_t i = 0; i < n; i += 16) copy16(dst + i, src + i);
                return pos + n;
            }
            if (d >= 8) {
                for (proven_size_t i = 0; i < n; i += 8) copy8(dst + i, src + i);
                return pos + n;
            }
            /* Closer than eight: the first bytes one at a time until the pattern of period `d`
             * is written out to `step`, the smallest multiple of `d` that is at least eight;
             * then eight at a time from `step` back, which repeats it. */
            const proven_size_t step = (proven_size_t)d * ((8 + (proven_size_t)d - 1) / (proven_size_t)d);
            proven_size_t i = 0;
            for (; i < step && i < n; ++i) dst[i] = src[i];
            for (; i < n; i += 8) copy8(dst + i, dst + i - step);
            return pos + n;
        }
    }
    while (n > 0) {
        const proven_size_t to = (proven_size_t)(pos & mask), from = (proven_size_t)((pos - d) & mask);
        proven_size_t k = n;
        if (k > cap - to) k = cap - to;
        if (k > cap - from) k = cap - from;
        proven_byte_t *dst = ring + to;
        const proven_byte_t *src = ring + from;
        proven_size_t i = 0;
        if (d >= 8) {
            /* Eight at a time: each eight read were written at least eight bytes before. */
            for (; i + 8 <= k; i += 8) copy8(dst + i, src + i);
        }
        for (; i < k; ++i) dst[i] = src[i];
        pos += k;
        n -= (proven_u32)k;
    }
    return pos;
}

/* The 64 literal codes of the current literal block type, one per context. */
static void literal_codes(const proven_brotli_decoder_t *b, const proven_u16 **codes) {
    const proven_u8 *map = b->cmap_l + 64 * b->c[0].type;
    for (unsigned i = 0; i < 64; ++i) codes[i] = b->tab + b->code_l[map[i]];
}

/* The one code all 64 contexts use, if they use one; else null. */
static const proven_u16 *single_code(const proven_u16 *const *codes) {
    for (unsigned i = 1; i < 64; ++i) if (codes[i] != codes[0]) return NULL;
    return codes[0];
}

/* Whole commands while there is input to spare (64 bytes, so no read can run out) and room
 * for the command's output: the items the step-by-step path takes one at a time, taken
 * without its checks for input, with the reader and the position in locals. It leaves the
 * state at an item the other path can go on from, and gives back the whole bytes it read
 * ahead. */
static void fast(proven_brotli_decoder_t *b, proven_io_t *io) {
    const proven_size_t start = io->in_at;
    const proven_byte_t *end = io->in + io->in_len;
    proven_category_t *ci = &b->c[1], *cl = &b->c[0], *cd = &b->c[2];
    const proven_u16 *codes[64];
    literal_codes(b, codes);
    const proven_u16 *one = single_code(codes);
    proven_brotli_reader_t r;
    sync_in(b, io, &r);
    while (b->state == S_COMMAND && end - r.p >= 64) {
        if (b->left == 0) { meta_end(b); break; }
        fill(&r);
        if (ci->count == 0) { sync_out(b, io, &r); (void)block_switch(b, io, ci); sync_in(b, io, &r); fill(&r); }
        unsigned len;
        const unsigned s = lookup(b->tab + b->code_i[ci->type], r.bits, &len);
        const unsigned cell = s >> 6;
        const unsigned ic = (cell < 2 ? 0u : CELL_INS[cell - 2]) + ((s >> 3) & 7u);
        const unsigned cc = (cell < 2 ? (cell == 1 ? 8u : 0u) : CELL_COPY[cell - 2]) + (s & 7u);
        r.bits >>= len;
        r.n -= len;
        const proven_u32 insert = INS_BASE[ic] + take_bits(&r, INS_EXTRA[ic]);
        fill(&r);
        const proven_u32 copy = COPY_BASE[cc] + take_bits(&r, COPY_EXTRA[cc]);
        ci->count--;
        b->insert = insert;
        b->copy = copy;
        b->copy_code = cc;
        b->implicit_zero = s < 128;
        b->state = S_LITERALS;
        if (insert > b->left) { bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
        /* Room for the literals, the copy or a word, and 64 bytes more (see copy_back). */
        const proven_u64 need = (proven_u64)insert + copy + 64;
        if (need > b->full) break;
        if (b->cap < b->full ? b->pos + need > b->cap : b->pos + need - b->flushed > b->cap) {
            sync_out(b, io, &r);
            const int k = room(b, io, (proven_size_t)need);
            if (k <= 0) { sync_in(b, io, &r); break; }
        }

        /* The literals. */
        proven_byte_t *ring = b->ring;
        const proven_size_t mask = b->cap - 1;
        proven_u64 pos = b->pos;
        /* The last two bytes, for the literals' contexts: read only when there are literals and
         * their code depends on them. */
        unsigned p1 = 0, p2 = 0;
        if (insert > 0 && !one) { p1 = pos >= 1 ? ring[(pos - 1) & mask] : 0; p2 = pos >= 2 ? ring[(pos - 2) & mask] : 0; }
        proven_u32 n = insert, count = cl->count;
        unsigned mode = b->cmode[cl->type];
        while (n > 0) {
            if (end - r.p < 16) break;
            fill(&r);
            if (count == 0) {
                sync_out(b, io, &r);
                (void)block_switch(b, io, cl);
                sync_in(b, io, &r);
                fill(&r);
                count = cl->count;
                mode = b->cmode[cl->type];
                literal_codes(b, codes);
                one = single_code(codes);
                p1 = pos >= 1 ? ring[(pos - 1) & mask] : 0;
                p2 = pos >= 2 ? ring[(pos - 2) & mask] : 0;
            }
            /* Up to three literals per fill: fifteen bits each. */
            unsigned batch = n < 3 ? n : 3;
            if (batch > count) batch = count;
            n -= batch;
            count -= batch;
            if (one) {
                /* No context: p1 and p2 are not kept, and are read back if a switch needs them. */
                for (unsigned k = 0; k < batch; ++k) { ring[pos & mask] = (proven_byte_t)take_sym(&r, one); pos++; }
                continue;
            }
            for (unsigned k = 0; k < batch; ++k) {
                unsigned ctx;
                switch (mode) {
                    case 0: ctx = p1 & 0x3fu; break;
                    case 1: ctx = p1 >> 2; break;
                    case 2: ctx = brotli_lut0[p1] | brotli_lut1[p2]; break;
                    default: ctx = (unsigned)(brotli_lut2[p1] << 3) | brotli_lut2[p2]; break;
                }
                const unsigned v = take_sym(&r, codes[ctx]);
                ring[pos & mask] = (proven_byte_t)v;
                pos++;
                p2 = p1;
                p1 = v;
            }
        }
        cl->count = count;
        b->pos = pos;
        b->left -= insert - n;
        b->insert = n;
        if (n > 0) break;
        if (b->left == 0) { meta_end(b); break; }

        /* The distance. */
        b->state = S_DISTANCE;
        if (!b->implicit_zero && end - r.p < 16) break;
        unsigned code = 0;
        proven_u32 extra = 0;
        if (!b->implicit_zero) {
            fill(&r);
            if (cd->count == 0) { sync_out(b, io, &r); (void)block_switch(b, io, cd); sync_in(b, io, &r); fill(&r); }
            const unsigned ctx = copy > 4 ? 3u : copy - 2;
            code = take_sym(&r, b->tab + b->code_d[b->cmap_d[4 * cd->type + ctx]]);
            extra = take_bits(&r, distance_extra(b, code));
            cd->count--;
            b->implicit_zero = true;            /* read: if the other path goes on from here, it reads none */
        }
        const proven_u32 d = distance_of(b, code, extra);
        if (d == 0) { bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
        const proven_u64 reach = pos < b->window ? pos : b->window;
        if (d > reach) {
            if (copy < 4 || copy > 24) { bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
            const proven_u64 id = d - reach - 1;
            const unsigned nbits = brotli_ndbits[copy];
            const proven_u64 transform = id >> nbits;
            if (transform >= 121) { bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
            dictionary_word(b, copy, (proven_u32)(id & ((1u << nbits) - 1u)), (unsigned)transform);
            if (b->word_len > b->left) { bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
            for (proven_u32 i = 0; i < b->word_len; ++i) ring[(pos + i) & mask] = b->word[i];
            b->pos = pos + b->word_len;
            b->left -= b->word_len;
            b->state = S_COMMAND;
            continue;
        }
        if (code != 0) { b->dist[b->dist_at & 3u] = d; b->dist_at++; }
        if (copy > b->left) { bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
        b->distance = d;
        b->pos = copy_back(ring, b->cap, pos, d, copy, true);
        b->left -= copy;
        b->copy = 0;
        b->state = S_COMMAND;
    }
    sync_out(b, io, &r);
    /* Whole bytes read ahead go back, as far as they were read here. */
    proven_size_t ahead = b->bitcnt >> 3;
    if (ahead > io->in_at - start) ahead = io->in_at - start;
    io->in_at -= ahead;
    b->bitcnt -= 8 * (unsigned)ahead;
    b->bitbuf &= ((proven_u64)1 << b->bitcnt) - 1u;
}

proven_err_t proven_brotli_decode(proven_brotli_decoder_t *b, proven_mem_view_t in, proven_size_t *consumed,
                                  proven_mem_mut_t out, proven_size_t *produced, bool *done) {
    if (consumed) *consumed = 0;
    if (produced) *produced = 0;
    if (done) *done = false;
    if (!b || !consumed || !produced || !done || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (b->error != PROVEN_OK) return b->error;
    proven_io_t io = { .in = in.ptr, .in_len = in.size, .in_at = 0, .out = out.ptr, .out_len = out.size, .out_at = 0 };
    proven_err_t result = PROVEN_OK;
    bool going = true;
    while (going && result == PROVEN_OK) {
        int r = 1;
        switch (b->state) {
            case S_WBITS:
                r = stream_header(b, &io);
                if (r > 0) b->state = S_HEADER;
                break;
            case S_HEADER:
                r = meta_header(b, &io);
                break;
            case S_METADATA:
                while (b->left > 0) {
                    proven_byte_t v;
                    if (!next_byte(b, &io, &v)) { r = 0; break; }
                    b->left--;
                }
                if (r > 0) meta_end(b);
                break;
            case S_UNCOMPRESSED:
                while (b->left > 0) {
                    const proven_size_t ask = b->cap < b->full ? (b->left < b->full - (proven_size_t)b->pos ? b->left : b->full - (proven_size_t)b->pos) : 1;
                    const int k = room(b, &io, ask);
                    if (k <= 0) { r = k; break; }
                    if (b->bitcnt >= 8) {
                        /* the whole bytes the bit buffer holds come first */
                        proven_byte_t v;
                        (void)next_byte(b, &io, &v);
                        put(b, v);
                        b->left--;
                        continue;
                    }
                    if (io.in_at == io.in_len) { r = 0; break; }
                    /* then straight from the input, as much as there is room for up to the ring's end */
                    const proven_size_t at = (proven_size_t)(b->pos & (b->cap - 1));
                    proven_size_t n = b->cap < b->full ? b->cap - (proven_size_t)b->pos : b->cap - (proven_size_t)(b->pos - b->flushed);
                    if (n > b->cap - at) n = b->cap - at;
                    if (n > b->left) n = b->left;
                    if (n > io.in_len - io.in_at) n = io.in_len - io.in_at;
                    (void)proven_mem_copy(b->ring + at, n, (proven_mem_view_t){ io.in + io.in_at, n });
                    io.in_at += n;
                    b->pos += n;
                    b->left -= (proven_u32)n;
                }
                if (r > 0) meta_end(b);
                break;
            case S_NBLTYPES: {
                proven_category_t *c = &b->c[b->cat];
                if (!var_count(b, &io, &c->n)) { r = 0; break; }
                c->type = 0;
                c->prev = 1;
                if (c->n < 2) {
                    c->count = NO_SWITCH;
                    b->state = b->cat == 2 ? S_DIST_PARAMS : S_NBLTYPES;
                    b->cat++;
                    break;
                }
                code_begin(b, c->n + 2);
                b->state = S_BTYPE_CODE;
                break; }
            case S_BTYPE_CODE:
                r = prefix_code(b, &io);
                if (r > 0) {
                    b->c[b->cat].type_code = b->pc_result;
                    code_begin(b, 26);
                    b->state = S_BCOUNT_CODE;
                }
                break;
            case S_BCOUNT_CODE:
                r = prefix_code(b, &io);
                if (r > 0) {
                    b->c[b->cat].count_code = b->pc_result;
                    b->state = S_BCOUNT;
                }
                break;
            case S_BCOUNT: {
                proven_category_t *c = &b->c[b->cat];
                if (!block_count(b, &io, b->tab + c->count_code, &c->count)) { r = 0; break; }
                b->state = b->cat == 2 ? S_DIST_PARAMS : S_NBLTYPES;
                b->cat++;
                break; }
            case S_DIST_PARAMS:
                if (!need(b, &io, 6)) { r = 0; break; }
                b->npostfix = peek(b, 0, 2);
                b->ndirect = peek(b, 2, 4) << b->npostfix;
                drop(b, 6);
                b->dist_alphabet = 16 + b->ndirect + (48u << b->npostfix);
                distance_codes(b);
                b->index = 0;
                b->state = S_CMODES;
                break;
            case S_CMODES:
                while (b->index < b->c[0].n) {
                    if (!need(b, &io, 2)) { r = 0; break; }
                    b->cmode[b->index++] = (proven_u8)peek(b, 0, 2);
                    drop(b, 2);
                }
                if (r > 0) { b->map_distance = false; b->state = S_NTREES; }
                break;
            case S_NTREES: {
                if (!var_count(b, &io, &b->ntrees)) { r = 0; break; }
                proven_u8 *map = b->map_distance ? b->cmap_d : b->cmap_l;
                const proven_u32 size = (b->map_distance ? 4u : 64u) * b->c[b->map_distance ? 2 : 0].n;
                if (b->map_distance) b->ntrees_d = b->ntrees; else b->ntrees_l = b->ntrees;
                if (b->ntrees < 2) {
                    for (proven_u32 i = 0; i < size; ++i) map[i] = 0;
                    if (b->map_distance) { b->index = 0; b->pc_alphabet = 0; b->state = S_TREES; }
                    else b->map_distance = true;
                    break;
                }
                b->state = S_RLEMAX;
                break; }
            case S_RLEMAX:
                if (!need(b, &io, 1)) { r = 0; break; }
                if (peek(b, 0, 1) == 0) { drop(b, 1); b->rlemax = 0; }
                else {
                    if (!need(b, &io, 5)) { r = 0; break; }
                    b->rlemax = peek(b, 1, 4) + 1;
                    drop(b, 5);
                }
                code_begin(b, b->ntrees + b->rlemax);
                b->state = S_CMAP_CODE;
                break;
            case S_CMAP_CODE:
                r = prefix_code(b, &io);
                if (r > 0) { b->map_code = b->pc_result; b->index = 0; b->state = S_CMAP; }
                break;
            case S_CMAP: {
                proven_u8 *map = b->map_distance ? b->cmap_d : b->cmap_l;
                const proven_u32 size = (b->map_distance ? 4u : 64u) * b->c[b->map_distance ? 2 : 0].n;
                const proven_u16 *t = b->tab + b->map_code;
                while (b->index < size) {
                    /* A value, or a run of zeros with its extra bits: taken together. */
                    unsigned len, s, extra;
                    for (;;) {
                        s = lookup(t, b->bitbuf, &len);
                        extra = s >= 1 && s <= b->rlemax ? s : 0;
                        if (len + extra <= b->bitcnt) break;
                        if (!load(b, &io)) { r = 0; break; }
                    }
                    if (r == 0) break;
                    if (s == 0 || s > b->rlemax) {
                        drop(b, len);
                        map[b->index++] = (proven_u8)(s == 0 ? 0 : s - b->rlemax);
                        continue;
                    }
                    const proven_u32 run = (1u << s) + peek(b, len, extra);
                    drop(b, len + extra);
                    if (run > size - b->index) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                    for (proven_u32 i = 0; i < run; ++i) map[b->index++] = 0;
                }
                if (r > 0 && result == PROVEN_OK) b->state = S_IMTF;
                break; }
            case S_IMTF: {
                if (!need(b, &io, 1)) { r = 0; break; }
                const bool imtf = peek(b, 0, 1) != 0;
                drop(b, 1);
                proven_u8 *map = b->map_distance ? b->cmap_d : b->cmap_l;
                const proven_u32 size = (b->map_distance ? 4u : 64u) * b->c[b->map_distance ? 2 : 0].n;
                if (imtf) {
                    proven_u8 mtf[256];
                    for (unsigned i = 0; i < 256; ++i) mtf[i] = (proven_u8)i;
                    for (proven_u32 i = 0; i < size; ++i) {
                        unsigned k = map[i];
                        const proven_u8 v = mtf[k];
                        map[i] = v;
                        for (; k; --k) mtf[k] = mtf[k - 1];
                        mtf[0] = v;
                    }
                }
                if (b->map_distance) { b->index = 0; b->pc_alphabet = 0; b->state = S_TREES; }
                else { b->map_distance = true; b->state = S_NTREES; }
                break; }
            case S_TREES: {
                /* The literal codes, then one insert-and-copy code per block type, then the
                 * distance codes: one index across the three lists. */
                const proven_u32 nl = b->ntrees_l, ni = b->c[1].n, nd = b->ntrees_d;
                while (b->index < nl + ni + nd) {
                    if (b->pc_alphabet == 0) {
                        code_begin(b, b->index < nl ? 256u : b->index < nl + ni ? 704u : b->dist_alphabet);
                    }
                    r = prefix_code(b, &io);
                    if (r <= 0) break;
                    const proven_u32 i = b->index++;
                    if (i < nl) b->code_l[i] = b->pc_result;
                    else if (i < nl + ni) b->code_i[i - nl] = b->pc_result;
                    else b->code_d[i - nl - ni] = b->pc_result;
                    b->pc_alphabet = 0;
                }
                if (r > 0) b->state = S_COMMAND;
                break; }
            case S_COMMAND: {
                if (io.in_len - io.in_at >= 64) {
                    fast(b, &io);
                    if (b->error != PROVEN_OK) { result = b->error; break; }
                    if (b->state != S_COMMAND || io.in_len - io.in_at >= 64) break;
                }
                if (b->left == 0) { meta_end(b); break; }
                proven_category_t *c = &b->c[1];
                if (c->count == 0 && !block_switch(b, &io, c)) { r = 0; break; }
                /* The insert-and-copy code and the insert length's extra bits, together. */
                const proven_u16 *t = b->tab + b->code_i[c->type];
                unsigned len, s, ic, cc;
                for (;;) {
                    s = lookup(t, b->bitbuf, &len);
                    const unsigned cell = s >> 6;
                    ic = (cell < 2 ? 0u : CELL_INS[cell - 2]) + ((s >> 3) & 7u);
                    cc = (cell < 2 ? (cell == 1 ? 8u : 0u) : CELL_COPY[cell - 2]) + (s & 7u);
                    if (len + INS_EXTRA[ic] <= b->bitcnt) break;
                    if (!load(b, &io)) { r = 0; break; }
                }
                if (r == 0) break;
                b->insert = INS_BASE[ic] + peek(b, len, INS_EXTRA[ic]);
                drop(b, len + INS_EXTRA[ic]);
                c->count--;
                b->copy_code = cc;
                b->implicit_zero = s < 128;
                if (b->insert > b->left) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                b->state = S_COPY_EXTRA;
                break; }
            case S_COPY_EXTRA:
                if (!need(b, &io, COPY_EXTRA[b->copy_code])) { r = 0; break; }
                b->copy = COPY_BASE[b->copy_code] + peek(b, 0, COPY_EXTRA[b->copy_code]);
                drop(b, COPY_EXTRA[b->copy_code]);
                b->state = S_LITERALS;
                break;
            case S_LITERALS: {
                proven_category_t *c = &b->c[0];
                while (b->insert > 0) {
                    const int k = room(b, &io, 1);
                    if (k <= 0) { r = k; break; }
                    if (c->count == 0 && !block_switch(b, &io, c)) { r = 0; break; }
                    const unsigned tree = b->cmap_l[64 * c->type + context(b, b->cmode[c->type])];
                    unsigned v;
                    if (!symbol(b, &io, b->tab + b->code_l[tree], &v)) { r = 0; break; }
                    c->count--;
                    put(b, (proven_byte_t)v);
                    b->insert--;
                    b->left--;
                }
                if (r <= 0) break;
                /* A meta-block that ends with these literals ignores the copy. */
                if (b->left == 0) { meta_end(b); break; }
                b->state = S_DISTANCE;
                break; }
            case S_DISTANCE: {
                unsigned code = 0;
                proven_u32 extra = 0;
                if (!b->implicit_zero) {
                    proven_category_t *c = &b->c[2];
                    if (c->count == 0 && !block_switch(b, &io, c)) { r = 0; break; }
                    const unsigned ctx = b->copy > 4 ? 3u : b->copy - 2;
                    const proven_u16 *t = b->tab + b->code_d[b->cmap_d[4 * c->type + ctx]];
                    unsigned len, nx;
                    for (;;) {
                        code = lookup(t, b->bitbuf, &len);
                        nx = distance_extra(b, code);
                        if (len + nx <= b->bitcnt) break;
                        if (!load(b, &io)) { r = 0; break; }
                    }
                    if (r == 0) break;
                    extra = peek(b, len, nx);
                    drop(b, len + nx);
                    c->count--;
                }
                const proven_u32 d = distance_of(b, code, extra);
                if (d == 0) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                const proven_u64 reach = b->pos < b->window ? b->pos : b->window;
                if (d > reach) {
                    /* A word of the static dictionary. */
                    if (b->copy < 4 || b->copy > 24) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                    const proven_u64 id = d - reach - 1;
                    const unsigned nbits = brotli_ndbits[b->copy];
                    const proven_u64 transform = id >> nbits;
                    if (transform >= 121) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                    dictionary_word(b, b->copy, (proven_u32)(id & ((1u << nbits) - 1u)), (unsigned)transform);
                    if (b->word_len > b->left) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                    b->state = S_WORD;
                    break;
                }
                if (code != 0) { b->dist[b->dist_at & 3u] = d; b->dist_at++; }
                if (b->copy > b->left) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                b->distance = d;
                b->state = S_COPY;
                break; }
            case S_COPY:
                /* As much at a time as the history has room for: while it grows, room for the
                 * whole copy (up to its final size); after, what the caller has taken. */
                while (b->copy > 0) {
                    const proven_size_t ask = b->cap < b->full ? (b->copy < b->full - (proven_size_t)b->pos ? b->copy : b->full - (proven_size_t)b->pos) : 1;
                    const int k = room(b, &io, ask);
                    if (k <= 0) { r = k; break; }
                    const proven_size_t avail = b->cap < b->full ? b->cap - (proven_size_t)b->pos : b->cap - (proven_size_t)(b->pos - b->flushed);
                    const proven_u32 n = b->copy < avail ? b->copy : (proven_u32)avail;
                    b->pos = copy_back(b->ring, b->cap, b->pos, b->distance, n, false);
                    b->copy -= n;
                    b->left -= n;
                }
                if (r > 0) b->state = S_COMMAND;
                break;
            case S_WORD: {
                const int k = room(b, &io, b->word_len - b->word_at);
                if (k <= 0) { r = k; break; }
                while (b->word_at < b->word_len) put(b, b->word[b->word_at++]);
                b->left -= b->word_len;
                b->state = S_COMMAND;
                break; }
            case S_END: {
                /* The rest of the last byte must be zeros; whole bytes in the buffer were never
                 * the stream's and go back to the caller. */
                if (peek(b, 0, b->bitcnt & 7u) != 0) { result = bad(b, PROVEN_ERR_INVALID_FORMAT); break; }
                io.in_at -= b->bitcnt >> 3;
                b->bitbuf = 0;
                b->bitcnt = 0;
                b->state = S_DONE;
                break; }
            default:
                going = false;
                break;
        }
        if (r < 0) result = b->error;
        else if (r == 0) going = false;
    }
    if (result == PROVEN_OK && b->ring) flush(b, &io);
    *consumed = io.in_at;
    *produced = io.out_at;
    *done = result == PROVEN_OK && b->state == S_DONE && b->flushed == b->pos;
    return result;
}

proven_err_t proven_brotli_decode_all(proven_allocator_t alloc, proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    if (written) *written = 0;
    if (!written || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (in.size == 0) return PROVEN_ERR_INVALID_FORMAT;
    proven_brotli_decoder_t *b = NULL;
    proven_err_t e = proven_brotli_decoder_create(alloc, 0, &b);
    if (e != PROVEN_OK) return e;
    proven_size_t used = 0, made = 0;
    bool done = false;
    e = proven_brotli_decode(b, in, &used, out, &made, &done);
    if (e == PROVEN_OK && !done && made == out.size) {
        /* The output is full and the stream has not ended: more data than allowed, or only the
         * end still to come. One more byte of room tells which. */
        proven_byte_t probe;
        proven_size_t more_in = 0, more_out = 0;
        e = proven_brotli_decode(b, (proven_mem_view_t){ .ptr = in.ptr + used, .size = in.size - used }, &more_in,
                                 (proven_mem_mut_t){ .ptr = &probe, .size = 1 }, &more_out, &done);
        used += more_in;
        if (e == PROVEN_OK && more_out > 0) e = PROVEN_ERR_OUT_OF_BOUNDS;
    }
    if (e == PROVEN_OK && (!done || used != in.size)) e = PROVEN_ERR_INVALID_FORMAT;
    proven_brotli_decoder_destroy(b);
    if (e != PROVEN_OK) return e;
    *written = made;
    return PROVEN_OK;
}

#endif /* PROVEN_NO_BROTLI */
