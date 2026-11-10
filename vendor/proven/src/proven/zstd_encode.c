#include "proven/zstd.h"
#include "proven_internal_zstd.h"
#if !defined(PROVEN_FREESTANDING) && (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#define ZSTD_BMI2 1
#include "../../platform/proven_sys_cpu.h"
#else
#define ZSTD_BMI2 0
#endif

/* Zstandard compression (RFC 8878).
 *
 * Input is matched a block (128 KiB at most) at a time against what came before it: a hash of
 * the next few bytes gives one earlier position to try, the three repeat offsets are tried
 * first, and the search skips ahead faster the longer it finds nothing. Each match becomes a
 * sequence - literals, then a match - and each block then gets its entropy coding: literals
 * by a Huffman code, the three kinds of sequence code by FSE tables, each table chosen for the
 * block by what it would cost. A block that would not shrink goes out stored.
 *
 * The whole-buffer path matches against the caller's input in place. The streaming path keeps
 * the window in its own buffer, which slides once full. Everything the format lets a decoder
 * carry from one block to the next - repeat offsets, the Huffman code, the FSE tables - is
 * kept here exactly as a decoder will have it, and put back when a block goes out stored. */

#define WINDOW_LOG_MIN 10
#define WINDOW_LOG_MAX 23
#define WINDOW_LOG_DEFAULT 19
#define LEVEL_MAX 19
#define LEVEL_DEFAULT 3
#define SEQ_MAX 16384u                  /* sequences in one block; a block ends early when they run out */
#define HUF_MAX_BITS 11
#define LL_LOG_MAX 9
#define ML_LOG_MAX 9
#define OF_LOG_MAX 8
#define SLACK 32                        /* bytes past the end of `lits` and of an output that eight-byte stores may touch */
#define NONE 0xFFFFFFFFu

// -----------------------------------------------------------------------------
// Bits written for a decoder that reads backwards
// -----------------------------------------------------------------------------

/* Bits go in lowest first; a flush stores eight bytes and moves on by the whole bytes among
 * them. `end` is eight bytes before the true end: a flush past it is clamped there and the
 * stream counts as too long. */
typedef struct {
    proven_byte_t *start, *p, *end;
    proven_u64 c;
    unsigned n;
} proven_bw_t;

ZSTD_HOT void st64(proven_byte_t *p, proven_u64 v) {
    p[0] = (proven_byte_t)v; p[1] = (proven_byte_t)(v >> 8); p[2] = (proven_byte_t)(v >> 16); p[3] = (proven_byte_t)(v >> 24);
    p[4] = (proven_byte_t)(v >> 32); p[5] = (proven_byte_t)(v >> 40); p[6] = (proven_byte_t)(v >> 48); p[7] = (proven_byte_t)(v >> 56);
}

static bool bw_init(proven_bw_t *b, proven_byte_t *dst, proven_size_t cap) {
    if (cap < 9) return false;
    b->start = b->p = dst;
    b->end = dst + cap - 8;
    b->c = 0;
    b->n = 0;
    return true;
}

/* `v` is below 2^nb. */
ZSTD_HOT void bw_add(proven_bw_t *b, proven_u64 v, unsigned nb) {
    b->c |= v << b->n;
    b->n += nb;
}

/* `v` has other bits above the `nb` that go. */
ZSTD_HOT void bw_add_low(proven_bw_t *b, proven_u64 v, unsigned nb) {
    static const proven_u32 MASK[33] = { 0, 1, 3, 7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
                                         0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF, 0x7FFFFF, 0xFFFFFF, 0x1FFFFFF, 0x3FFFFFF,
                                         0x7FFFFFF, 0xFFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF };
    b->c |= (v & MASK[nb]) << b->n;
    b->n += nb;
}

ZSTD_HOT void bw_flush(proven_bw_t *b) {
    st64(b->p, b->c);
    const unsigned k = b->n >> 3;
    b->p += k;
    b->c >>= 8 * k;                     /* k is at most 7: fewer than 64 bits are ever held */
    b->n &= 7;
    if (b->p > b->end) b->p = b->end;
}

/* The end marker, and the length; 0 when the stream did not fit. */
static proven_size_t bw_close(proven_bw_t *b) {
    bw_add(b, 1, 1);
    bw_flush(b);
    if (b->p >= b->end) return 0;
    return (proven_size_t)(b->p - b->start) + (b->n > 0);
}

// -----------------------------------------------------------------------------
// FSE tables, the encoding side (RFC 8878, section 4.1)
// -----------------------------------------------------------------------------

/* For each symbol: how many bits leave the state, and where its next states begin. */
typedef struct {
    proven_u16 state[1u << LL_LOG_MAX];
    struct { proven_u32 delta; proven_i32 find; } tt[64];
    proven_i16 norm[64];
    unsigned nsym, log;
} proven_fse_tab_t;

/* The same spread of symbols over states as a decoder makes (zstd.c, fse_build). */
static void fse_tab_build(proven_fse_tab_t *t, const proven_i16 *norm, unsigned nsym, unsigned log) {
    const proven_u32 size = 1u << log, mask = size - 1;
    proven_u32 high = size - 1, cumul[65];
    proven_byte_t spread[1u << LL_LOG_MAX];
    t->nsym = nsym;
    t->log = log;
    cumul[0] = 0;
    for (unsigned s = 0; s < nsym; ++s) {
        t->norm[s] = norm[s];
        if (norm[s] == -1) { cumul[s + 1] = cumul[s] + 1; spread[high--] = (proven_byte_t)s; }
        else cumul[s + 1] = cumul[s] + (proven_u32)norm[s];
    }
    const proven_u32 step = (size >> 1) + (size >> 3) + 3;
    proven_u32 pos = 0;
    for (unsigned s = 0; s < nsym; ++s) {
        for (int i = 0; i < norm[s]; ++i) {
            spread[pos] = (proven_byte_t)s;
            do pos = (pos + step) & mask; while (pos > high);
        }
    }
    for (proven_u32 u = 0; u < size; ++u) t->state[cumul[spread[u]]++] = (proven_u16)(size + u);
    proven_i32 total = 0;
    for (unsigned s = 0; s < nsym; ++s) {
        if (norm[s] == 0) {
            t->tt[s].delta = ((log + 1) << 16) - size;
            t->tt[s].find = 0;
        } else if (norm[s] == -1 || norm[s] == 1) {
            t->tt[s].delta = (log << 16) - size;
            t->tt[s].find = total - 1;
            total++;
        } else {
            const unsigned out = log - highbit32((proven_u32)norm[s] - 1);
            t->tt[s].delta = (out << 16) - ((proven_u32)norm[s] << out);
            t->tt[s].find = total - norm[s];
            total += norm[s];
        }
    }
}

/* One symbol only, in every state: no bits at all. */
static void fse_tab_rle(proven_fse_tab_t *t, unsigned sym) {
    t->state[0] = 0;
    t->state[1] = 0;
    t->tt[sym].delta = 0;
    t->tt[sym].find = 0;
    for (unsigned s = 0; s <= sym; ++s) t->norm[s] = 0;
    t->norm[sym] = 1;
    t->nsym = sym + 1;
    t->log = 0;
}

ZSTD_HOT proven_u32 fse_first(const proven_fse_tab_t *t, unsigned s) {
    const proven_u32 nb = (t->tt[s].delta + (1u << 15)) >> 16;
    const proven_u32 v = (nb << 16) - t->tt[s].delta;
    return t->state[(proven_i32)(v >> nb) + t->tt[s].find];
}

ZSTD_HOT void fse_put(proven_bw_t *b, proven_u32 *v, const proven_fse_tab_t *t, unsigned s) {
    const proven_u32 nb = (*v + t->tt[s].delta) >> 16;
    bw_add_low(b, *v, nb);
    *v = t->state[(proven_i32)(*v >> nb) + t->tt[s].find];
}

/* The smallest table that serves `total` symbols of `maxsym + 1` kinds well, at most `max`
 * (the reference encoder's rule). */
static unsigned fse_log(proven_size_t total, unsigned maxsym, unsigned max) {
    const unsigned by_src = highbit32((proven_u32)(total - 1)) - 2u;
    const unsigned need_src = highbit32((proven_u32)total) + 1, need_sym = highbit32(maxsym) + 2;
    const unsigned need = need_src < need_sym ? need_src : need_sym;
    unsigned log = max;
    if (total > 4 && by_src < log) log = by_src;
    if (need > log) log = need;
    if (log < 5) log = 5;
    if (log > max) log = max;
    return log;
}

/* Counts to probabilities that sum to 2^log, no present symbol below 1 (the reference
 * encoder's rounding, with a plain fallback when that overshoots). False when even the
 * fallback cannot balance them; the caller then uses another kind of table. */
static bool fse_normalize(proven_i16 *norm, unsigned log, const proven_u32 *count, proven_size_t total, unsigned maxsym) {
    static const proven_u32 RTB[8] = { 0, 473195, 504333, 520860, 550000, 700000, 750000, 830000 };
    const unsigned scale = 62 - log;
    const proven_u64 step = ((proven_u64)1 << 62) / total, vstep = (proven_u64)1 << (scale - 20);
    const proven_u32 low = (proven_u32)(total >> log);
    int still = 1 << log;
    unsigned largest = 0;
    proven_i16 largest_p = 0;
    while (largest < maxsym && count[largest] == 0) largest++;
    for (unsigned s = 0; s <= maxsym; ++s) {
        if (count[s] == 0) { norm[s] = 0; continue; }
        if (count[s] <= low) { norm[s] = 1; still--; continue; }
        proven_i16 p = (proven_i16)((count[s] * step) >> scale);
        if (p < 8) p = (proven_i16)(p + ((count[s] * step) - ((proven_u64)p << scale) > vstep * RTB[p]));
        if (p > largest_p) { largest_p = p; largest = s; }
        norm[s] = p;
        still -= p;
    }
    if (-still < (norm[largest] >> 1)) { norm[largest] = (proven_i16)(norm[largest] + still); return true; }
    /* Rounding gave too much away: proportional shares rounded down, at least 1 each, then
     * the remainder to the largest and any excess taken from the largest shares. */
    int sum = 0;
    for (unsigned s = 0; s <= maxsym; ++s) {
        if (count[s] == 0) continue;
        proven_u64 p = ((proven_u64)count[s] << log) / total;
        norm[s] = (proven_i16)(p ? p : 1);
        sum += norm[s];
    }
    still = (1 << log) - sum;
    while (still < 0) {
        unsigned big = largest;
        for (unsigned s = 0; s <= maxsym; ++s) if (norm[s] > norm[big]) big = s;
        if (norm[big] <= 1) return false;
        norm[big]--;
        still++;
    }
    norm[largest] = (proven_i16)(norm[largest] + still);
    return true;
}

/* The table's description (section 4.1.1), lowest bits first. 0 when it does not fit. */
static proven_size_t fse_describe(proven_byte_t *d, proven_size_t cap, const proven_i16 *norm, unsigned maxsym, unsigned log) {
    proven_u64 bits = 0;
    unsigned nbits = 0;
    proven_size_t o = 0;
#define PUT(v, n) do { bits |= (proven_u64)(v) << nbits; nbits += (n); while (nbits >= 8) { if (o >= cap) return 0; d[o++] = (proven_byte_t)bits; bits >>= 8; nbits -= 8; } } while (0)
    PUT(log - 5, 4);
    int remaining = (1 << log) + 1, threshold = 1 << log;
    unsigned nb = log + 1, s = 0;
    bool prev0 = false;
    while (s <= maxsym && remaining > 1) {
        if (prev0) {
            unsigned from = s;
            while (s <= maxsym && norm[s] == 0) s++;
            if (s > maxsym) return 0;
            while (s >= from + 24) { from += 24; PUT(0xFFFFu, 16); }
            while (s >= from + 3) { from += 3; PUT(3u, 2); }
            PUT(s - from, 2);
        }
        int count = norm[s++];
        const int max = (2 * threshold - 1) - remaining;
        remaining -= count < 0 ? -count : count;
        count++;
        if (count >= threshold) count += max;
        PUT((unsigned)count, nb - (count < max ? 1u : 0u));
        prev0 = count == 1;
        if (remaining < 1) return 0;
        while (remaining < threshold) { nb--; threshold >>= 1; }
    }
#undef PUT
    if (remaining != 1) return 0;
    if (nbits > 0) { if (o >= cap) return 0; d[o++] = (proven_byte_t)bits; }
    return o;
}

/* log2(n), in 256ths of a bit, near enough to compare costs. */
static inline proven_u32 log2_256(proven_u32 n) {
    const unsigned hb = highbit32(n);
    return (hb << 8) + ((n << 8) >> hb) - 256;
}

/* What `count` costs in a table, in 256ths of a bit; NONE when a symbol has no state there. */
static proven_u64 fse_cost(const proven_u32 *count, unsigned maxsym, const proven_i16 *norm, unsigned nsym, unsigned log) {
    proven_u64 c = 0;
    for (unsigned s = 0; s <= maxsym; ++s) {
        if (count[s] == 0) continue;
        if (s >= nsym || norm[s] == 0) return ~(proven_u64)0;
        const proven_u32 p = norm[s] < 0 ? 1u : (proven_u32)norm[s];
        c += (proven_u64)count[s] * ((log << 8) - log2_256(p));
    }
    return c;
}

// -----------------------------------------------------------------------------
// Huffman codes for literals (section 4.2)
// -----------------------------------------------------------------------------

typedef struct {
    proven_u16 code[256];
    proven_u8 bits[256];               /* 0: the symbol has no code */
    proven_u32 both[256];              /* bits << 16 | code: what the stream writer takes in one load */
    unsigned maxsym;
} proven_huf_tab_t;

typedef struct { proven_u32 key; proven_u16 sym; } proven_symfreq_t;

/* Code lengths of minimum redundancy for frequencies sorted in rising order (Moffat and
 * Katajainen, in place): on return each key is that symbol's code length. */
static void minimum_redundancy(proven_symfreq_t *a, int n) {
    if (n == 1) { a[0].key = 1; return; }
    int root = 0, leaf = 2, next;
    a[0].key += a[1].key;
    for (next = 1; next < n - 1; ++next) {
        if (leaf >= n || a[root].key < a[leaf].key) { a[next].key = a[root].key; a[root++].key = (proven_u32)next; }
        else a[next].key = a[leaf++].key;
        if (leaf >= n || (root < next && a[root].key < a[leaf].key)) { a[next].key += a[root].key; a[root++].key = (proven_u32)next; }
        else a[next].key += a[leaf++].key;
    }
    a[n - 2].key = 0;
    for (next = n - 3; next >= 0; --next) a[next].key = a[a[next].key].key + 1;
    int avail = 1, used = 0, depth = 0;
    root = n - 2; next = n - 1;
    while (avail > 0) {
        while (root >= 0 && (int)a[root].key == depth) { used++; root--; }
        while (avail > used) { a[next--].key = (proven_u32)depth; avail--; }
        avail = 2 * used; depth++; used = 0;
    }
}

/* A code of at most HUF_MAX_BITS bits for at least two symbols, and its codes as a decoder
 * builds them from the weights: by weight, then by symbol (zstd.c, huf_read). */
static void huf_build(proven_huf_tab_t *t, const proven_u32 *count, unsigned maxsym) {
    proven_symfreq_t a[256];
    int used = 0, counts[HUF_MAX_BITS + 2] = { 0 };
    for (unsigned s = 0; s <= maxsym; ++s) {
        t->bits[s] = 0;
        if (count[s] == 0) continue;
        proven_u32 k = count[s];
        int j = used++;
        while (j > 0 && a[j - 1].key > k) { a[j] = a[j - 1]; --j; }
        a[j] = (proven_symfreq_t){ k, (proven_u16)s };
    }
    minimum_redundancy(a, used);
    for (int i = 0; i < used; ++i) counts[a[i].key > HUF_MAX_BITS ? HUF_MAX_BITS : a[i].key]++;
    proven_u32 total = 0;
    for (int i = HUF_MAX_BITS; i > 0; --i) total += (proven_u32)counts[i] << (HUF_MAX_BITS - i);
    while (total != (1u << HUF_MAX_BITS)) {
        counts[HUF_MAX_BITS]--;
        for (int i = HUF_MAX_BITS - 1; i > 0; --i) if (counts[i]) { counts[i]--; counts[i + 1] += 2; break; }
        total--;
    }
    int at = 0;
    unsigned log = 0;
    for (int b = HUF_MAX_BITS; b > 0; --b) {
        if (counts[b] && (unsigned)b > log) log = (unsigned)b;
        for (int k = 0; k < counts[b]; ++k) t->bits[a[at++].sym] = (proven_u8)b;
    }
    /* Weight w = log + 1 - bits; the codes of weight w begin where those of lower weights end. */
    proven_u32 rank[HUF_MAX_BITS + 2] = { 0 }, next[HUF_MAX_BITS + 2];
    for (unsigned s = 0; s <= maxsym; ++s) if (t->bits[s]) rank[log + 1 - t->bits[s]]++;
    proven_u32 pos = 0;
    for (unsigned w = 1; w <= log; ++w) { next[w] = pos; pos += rank[w] << (w - 1); }
    for (unsigned s = 0; s <= maxsym; ++s) {
        if (!t->bits[s]) continue;
        const unsigned w = log + 1 - t->bits[s];
        t->code[s] = (proven_u16)(next[w] >> (w - 1));
        next[w] += 1u << (w - 1);
    }
    for (unsigned s = 0; s <= maxsym; ++s) t->both[s] = t->bits[s] ? ((proven_u32)t->bits[s] << 16) | t->code[s] : 0;
    t->maxsym = maxsym;
}

/* A dictionary's code: from its weights (the last included), as a decoder builds it. */
static void huf_from_weights(proven_huf_tab_t *t, const proven_u8 *w, unsigned n, unsigned log) {
    proven_u32 rank[HUF_LOG_MAX + 2] = { 0 }, next[HUF_LOG_MAX + 2];
    for (unsigned s = 0; s < 256; ++s) t->bits[s] = s < n && w[s] ? (proven_u8)(log + 1 - w[s]) : 0;
    for (unsigned s = 0; s < n; ++s) if (w[s]) rank[w[s]]++;
    proven_u32 pos = 0;
    for (unsigned k = 1; k <= log; ++k) { next[k] = pos; pos += rank[k] << (k - 1); }
    for (unsigned s = 0; s < n; ++s) {
        if (!w[s]) continue;
        t->code[s] = (proven_u16)(next[w[s]] >> (w[s] - 1));
        next[w[s]] += 1u << (w[s] - 1);
    }
    for (unsigned s = 0; s < 256; ++s) t->both[s] = t->bits[s] ? ((proven_u32)t->bits[s] << 16) | t->code[s] : 0;
    t->maxsym = n - 1;
}

static unsigned huf_log(const proven_huf_tab_t *t) {
    unsigned log = 0;
    for (unsigned s = 0; s <= t->maxsym; ++s) if (t->bits[s] > log) log = t->bits[s];
    return log;
}

/* Weights, FSE-coded with two states taking turns (as a decoder reads them). */
static proven_size_t huf_weights_fse(proven_byte_t *d, proven_size_t cap, const proven_byte_t *w, unsigned n) {
    proven_u32 count[16] = { 0 };
    unsigned maxw = 0, most = 0;
    for (unsigned i = 0; i < n; ++i) { count[w[i]]++; if (w[i] > maxw) maxw = w[i]; }
    for (unsigned k = 0; k <= maxw; ++k) if (count[k] > most) most = count[k];
    if (n < 2 || most == n || most == 1) return 0;
    const unsigned log = fse_log(n, maxw, 6);
    proven_i16 norm[16];
    if (!fse_normalize(norm, log, count, n, maxw)) return 0;
    proven_size_t o = fse_describe(d, cap, norm, maxw, log);
    if (o == 0) return 0;
    proven_fse_tab_t t;
    fse_tab_build(&t, norm, maxw + 1, log);
    proven_bw_t b;
    if (!bw_init(&b, d + o, cap - o)) return 0;
    const proven_byte_t *ip = w + n;
    proven_u32 s1, s2;
    if (n & 1) {
        s1 = fse_first(&t, *--ip);
        s2 = fse_first(&t, *--ip);
        fse_put(&b, &s1, &t, *--ip);
        bw_flush(&b);
    } else {
        s2 = fse_first(&t, *--ip);
        s1 = fse_first(&t, *--ip);
    }
    while (ip > w) {
        fse_put(&b, &s2, &t, *--ip);
        fse_put(&b, &s1, &t, *--ip);
        bw_flush(&b);
    }
    bw_add_low(&b, s2, log);
    bw_add_low(&b, s1, log);
    const proven_size_t k = bw_close(&b);
    return k ? o + k : 0;
}

/* The table's description: weights FSE-coded when that is shorter, else four bits each. */
static proven_size_t huf_describe(proven_byte_t *d, proven_size_t cap, const proven_huf_tab_t *t) {
    const unsigned log = huf_log(t), n = t->maxsym;   /* the last symbol's weight is implied */
    proven_byte_t w[256];
    for (unsigned s = 0; s < n; ++s) w[s] = t->bits[s] ? (proven_byte_t)(log + 1 - t->bits[s]) : 0;
    if (cap < 2) return 0;
    const proven_size_t f = huf_weights_fse(d + 1, cap - 1 < 128 ? cap - 1 : 127, w, n);
    const proven_size_t direct = n <= 128 ? 1 + (n + 1) / 2 : 0;
    if (f > 0 && f < 128 && (direct == 0 || 1 + f < direct)) {
        d[0] = (proven_byte_t)f;
        return 1 + f;
    }
    if (direct == 0 || direct > cap) return 0;
    d[0] = (proven_byte_t)(127 + n);
    for (unsigned i = 0; i < n; i += 2) d[1 + i / 2] = (proven_byte_t)((w[i] << 4) | (i + 1 < n ? w[i + 1] : 0));
    return direct;
}

/* One Huffman stream: the last symbol first, so that a decoder reading backwards meets the
 * first one first. */
ZSTD_HOT proven_size_t huf_stream(proven_byte_t *d, proven_size_t cap, const proven_byte_t *src, proven_size_t n, const proven_huf_tab_t *t) {
    proven_bw_t b;
    if (!bw_init(&b, d, cap)) return 0;
    const proven_u32 *const e = t->both;
    proven_size_t i = n;
#define HUF_PUT(x) do { const proven_u32 v_ = e[x]; bw_add(&b, v_ & 0xFFFF, v_ >> 16); } while (0)
    while (i & 3) { --i; HUF_PUT(src[i]); }
    bw_flush(&b);
    while (i > 0) {
        i -= 4;
        HUF_PUT(src[i + 3]);
        HUF_PUT(src[i + 2]);
        HUF_PUT(src[i + 1]);
        HUF_PUT(src[i]);
        bw_flush(&b);
    }
#undef HUF_PUT
    return bw_close(&b);
}

// -----------------------------------------------------------------------------
// The encoder
// -----------------------------------------------------------------------------

typedef struct {
    proven_u32 ofv;                    /* offset value as a decoder reads it: 1 to 3 a repeat, else offset + 3 */
    proven_u32 ll, ml;
} proven_seq_t;

/* What a decoder carries from block to block, besides the repeat offsets: the Huffman code
 * and a table for each kind of sequence code. `cur` names the slot in force (-1: none yet;
 * for the sequence tables 2: the predefined one); a block builds its new tables in the other
 * slot and, if it goes out compressed, makes that slot the one in force. */
typedef struct {
    proven_huf_tab_t huf[2];
    int huf_cur;
    proven_fse_tab_t tab[3][2];
    int cur[3];
} proven_entropy_t;

enum { K_LL, K_OF, K_ML };

typedef struct {
    unsigned strategy;                 /* 0: one hash table; 1: two; 2: a hash table and a chain; 3: and binary trees, lazy2; 4: the trees, optimal parsing */
    unsigned hlog, clog, mls;          /* clog: the second table's, or the chain's, size */
    unsigned tries, depth;             /* chain: candidates looked at; 0 greedy, 1 lazy, 2 lazy2. Optimal parsing: 1 also finds three-byte matches */
    unsigned target;                   /* optimal parsing: a match this long is taken at once */
} proven_params_t;

/* Long-distance matching, as libzstd's: a gear hash of the last 64 bytes picks one position in about
 * 2^LDM_RATE_LOG; those are kept in buckets of 2^LDM_BUCKET_LOG, and a match of LDM_MIN bytes or more
 * found there is taken - the level's own matcher fills what lies between such matches. */
#define LDM_MIN 64
#define LDM_RATE_LOG 7
#define LDM_BUCKET_LOG 3
#define LDM_LEVEL_MAX 5                 /* the levels it is used at */
struct proven_ldm_match { proven_u32 at, off, len; };

/* Optimal parsing (levels 16 to 19). Prices are in 1/256 of a bit. */
#define OPT_NUM 4096                   /* positions one stretch of the path may cover */
#define OPT_MATCHES 72                 /* matches found at one position: three repeats, a three-byte one, and the trees' */
#define OPT_H3_LOG 16                  /* the table of three-byte matches (levels whose `depth` is 1) */
#define OPT_H3_REACH ((proven_u32)1 << 18)   /* nearer than this: a three-byte match costs less than its literals */
typedef struct { proven_u32 price, dist, mlen, litlen, rep[3]; } proven_opt_node_t;
typedef struct { proven_u32 offv, dist, len; } proven_opt_match_t;
struct proven_opt {
    bool ready;                        /* statistics carried over from an earlier block of the frame */
    proven_u32 lit[256], ll[36], ml[53], of[32];
    proven_u32 lit_sum, ll_sum, ml_sum, of_sum;
    proven_u32 lit_w, ll_w, ml_w, of_w;   /* the sums' weights */
    proven_u32 lit_p[256], ll_p[36], ml_p[53], of_p[32];   /* each symbol's price */
    proven_opt_match_t m[OPT_MATCHES];
    proven_u32 h3_next;                /* the first position not yet in `h3` */
    unsigned h3_log;                   /* OPT_H3_LOG, or less for a short input */
    proven_u32 span_max;               /* OPT_NUM, or less for a short input */
    proven_opt_node_t *node;           /* span_max + 2 of them, after this struct */
    proven_u32 *h3;                    /* 2^h3_log: the last position of each hash of three bytes */
};

struct proven_zstd_encoder {
    proven_allocator_t alloc;
    proven_zstd_encoder_options_t opt;
    proven_params_t p;
    unsigned wlog;
    proven_size_t window, block_size;
    /* the frame */
    bool header_done, finished, failed;
    proven_u64 total;
    proven_u64 stated;                 /* the size the frame states (proven_zstd_encode_all through the buffer); 0: none */
    proven_xxh64_t xxh;
    proven_u32 rep[3];
    proven_entropy_t ent;
    proven_fse_tab_t def[3];                  /* the predefined tables, built when a block first uses one */
    bool def_built[3];
    proven_size_t seq_max, lit_max;    /* the most sequences and literals one block can have */
    /* matching */
    proven_u32 *hash, *hash2;
    proven_size_t hash_size, hash2_size;
    bool fresh;                        /* the hash tables hold nothing of the current frame */
    proven_u32 next_update;            /* chain: the first position not yet in the tables */
    struct proven_opt *optp;           /* optimal parsing (levels 16 to 19): its statistics and its path */
    proven_seq_t *seqs;
    proven_byte_t *codes;              /* each sequence's literal-length, offset and match-length codes */
    proven_size_t nseq;
    proven_byte_t *lits;
    proven_size_t nlit;
    /* the streaming buffer: the window, then input not yet compressed */
    proven_byte_t *buf;
    proven_size_t buf_cap, buf_max, pos, fill;
    /* output waiting to be handed over */
    proven_byte_t *pend;
    proven_size_t pend_cap, pend_len, pend_at;
    /* long-distance matching (`long_distance`): buckets of LDM_BUCKET entries, each a position (low
     * 32 bits) and a check of the 64 bytes there (high 32), and the next slot of each bucket; the
     * matches found in the block being matched */
    proven_u64 *ldm;
    proven_u8 *ldm_next;
    unsigned ldm_log;                  /* entries: 2^ldm_log */
    struct proven_ldm_match *ldm_m;
    proven_size_t ldm_cap;
    /* a dictionary (`dictionary`): its content is the past every frame starts from - positions
     * [0, dict_at) of the frame's buffer while matches may still reach it, 0 after a slide - and a
     * formatted one's repeat offsets and tables are where the first block starts */
    struct proven_zstd_cdict *cd;
    proven_size_t dict_at;
};

/* A dictionary as the encoder keeps it: as read, and a formatted one's tables built as a decoder
 * builds them, for the first block to repeat. */
struct proven_zstd_cdict {
    proven_zstd_dict_t_ d;
    proven_huf_tab_t huf;
    proven_fse_tab_t tab[3];
};

static const proven_params_t LEVELS[LEVEL_MAX + 1] = {
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 14, 0, 6, 0, 0, 0 },
    { 1, 14, 13, 6, 0, 0, 0 },
    { 1, 17, 16, 5, 0, 0, 0 },
    { 2, 17, 16, 5, 4, 0, 0 },            /* greedy */
    { 2, 17, 17, 5, 8, 0, 0 },
    { 2, 17, 17, 5, 12, 1, 0 },           /* lazy: 12 candidates, where libzstd's level 6 has 8, to stay at or below gzip 6 */
    { 2, 17, 17, 5, 12, 2, 0 },           /* lazy2 */
    { 2, 17, 17, 5, 24, 2, 0 },
    { 2, 17, 17, 4, 32, 2, 0 },
    /* 10 to 12 (RFC-0038 Z-1): libzstd's larger tables for them, each cut to the window and a known
     * input as every level's; four-byte hashes (libzstd's are five: four is smaller on binaries and
     * JSON here, within a point on C) and more candidates. */
    { 2, 22, 21, 4, 48, 2, 0 },
    { 2, 22, 21, 4, 96, 2, 0 },
    { 2, 23, 22, 4, 128, 2, 0 },
    /* 13 to 15 (RFC-0038 Z-2): the same lazy2 parse over binary trees, with libzstd's sizes for these
     * levels; `tries` is how many nodes a walk compares. */
    { 3, 22, 22, 4, 32, 2, 0 },
    { 3, 23, 22, 4, 64, 2, 0 },
    { 3, 23, 23, 4, 128, 2, 0 },
    /* 16 to 19 (RFC-0038 Z-3): the trees, every match they find weighed by what it would cost to
     * write, and the cheapest path through each stretch of input taken; 18 and 19 also take
     * three-byte matches. Below 256 KiB of known input 13 to 19 all parse so (make). */
    { 4, 22, 22, 4, 32, 0, 48 },
    { 4, 22, 23, 4, 32, 0, 64 },
    { 4, 22, 23, 4, 64, 1, 64 },
    { 4, 22, 24, 4, 128, 1, 256 },
};

/* Levels 6 to 9 for a known input above 256 KiB (proven_zstd_encode_all): libzstd's hash and chain
 * sizes for such inputs (hash, chain), cut to the window as every level's. The servers, which stream
 * at these levels, keep the tables above. */
static const unsigned LEVELS_BIG[4][2] = { { 19, 18 }, { 20, 19 }, { 20, 19 }, { 21, 20 } };

static proven_err_t grow(proven_zstd_encoder_t *z, void **buf, proven_size_t *cap, proven_size_t want, proven_size_t unit) {
    if (want <= *cap && *buf) return PROVEN_OK;
    proven_result_mem_mut_t m = *buf ? z->alloc.realloc_fn(z->alloc.ctx, *buf, *cap * unit, want * unit, 16)
                                     : z->alloc.alloc_fn(z->alloc.ctx, want * unit, 16);
    if (m.err != PROVEN_OK) return PROVEN_ERR_NOMEM;
    *buf = m.value.ptr;
    *cap = want;
    return PROVEN_OK;
}

static void frame_start(proven_zstd_encoder_t *z) {
    if (z->optp) z->optp->ready = false;      /* a new frame starts its statistics again */
    z->header_done = false;
    z->finished = false;
    z->failed = false;
    z->total = 0;
    xxh_init(&z->xxh);
    z->rep[0] = 1; z->rep[1] = 4; z->rep[2] = 8;
    z->ent.huf_cur = -1;
    z->ent.cur[0] = z->ent.cur[1] = z->ent.cur[2] = -1;
    z->dict_at = 0;
    if (z->cd) {
        z->rep[0] = z->cd->d.rep[0]; z->rep[1] = z->cd->d.rep[1]; z->rep[2] = z->cd->d.rep[2];
        if (z->cd->d.formatted) {
            z->ent.huf[0] = z->cd->huf;
            z->ent.huf_cur = 0;
            for (int k = 0; k < 3; ++k) { z->ent.tab[k][0] = z->cd->tab[k]; z->ent.cur[k] = 0; }
        }
    }
    z->pos = z->fill = 0;
    z->pend_len = z->pend_at = 0;
    if (!z->fresh) {
        for (proven_size_t i = 0; i < z->hash_size; ++i) z->hash[i] = 0;
        for (proven_size_t i = 0; i < z->hash2_size; ++i) z->hash2[i] = 0;
        z->fresh = true;
    }
    z->next_update = 0;
    if (z->ldm) {
        for (proven_size_t i = 0; i < ((proven_size_t)1 << z->ldm_log); ++i) z->ldm[i] = 0;
        for (proven_size_t i = 0; i < ((proven_size_t)1 << (z->ldm_log - LDM_BUCKET_LOG)); ++i) z->ldm_next[i] = 0;
    }
    if (z->optp) {
        for (proven_size_t i = 0; i < ((proven_size_t)1 << z->optp->h3_log); ++i) z->optp->h3[i] = 0;
        z->optp->h3_next = 0;
    }
}

static proven_err_t make(const proven_zstd_encoder_options_t *o, proven_u64 size_hint, proven_zstd_encoder_t **out) {
    if (!out) return PROVEN_ERR_INVALID_ARG;
    *out = NULL;
    if (!o || !proven_alloc_is_valid(o->alloc) || o->level < 0 || o->level > LEVEL_MAX ||
        (o->window_log != 0 && (o->window_log < WINDOW_LOG_MIN || o->window_log > WINDOW_LOG_MAX))) return PROVEN_ERR_INVALID_ARG;
    proven_result_mem_mut_t m = o->alloc.alloc_fn(o->alloc.ctx, sizeof(proven_zstd_encoder_t), 16);
    if (m.err != PROVEN_OK) return PROVEN_ERR_NOMEM;
    proven_zstd_encoder_t *z = (proven_zstd_encoder_t *)(void *)m.value.ptr;
    *z = (proven_zstd_encoder_t){ .alloc = o->alloc, .opt = *o };
    if (z->opt.level == 0) z->opt.level = LEVEL_DEFAULT;
    z->wlog = o->window_log ? (unsigned)o->window_log : WINDOW_LOG_DEFAULT;
    z->p = LEVELS[z->opt.level];
    proven_zstd_dict_t_ dr;
    proven_size_t dlen = 0;
    if (o->dictionary.size > 0 || o->dictionary.ptr) {
        const proven_err_t de = zstd_dict_read(o->dictionary, &dr);
        if (de != PROVEN_OK) { o->alloc.free_fn(o->alloc.ctx, z); return de; }
        dlen = dr.content_len;
    }
    /* A known size smaller than the window needs no more window, nor larger tables (the
     * dictionary's content is matched against too). */
    unsigned need = z->wlog;
    if (size_hint != ~(proven_u64)0) {
        need = WINDOW_LOG_MIN;
        while (need < z->wlog && ((proven_u64)1 << need) < size_hint + dlen) need++;
    }
    /* A known input of 256 KiB or less: from level 13 on, the optimal parse with three-byte matches,
     * as libzstd's table for such inputs has it (its trees and lazy parse only pay on more). */
    if (size_hint <= ((proven_u64)256 << 10) && z->p.strategy >= 3) {
        if (z->p.strategy == 3) z->p.target = 48;
        else { z->p.tries *= 4; z->p.target *= 2; }   /* 16 to 19 look further than 13 to 15 */
        z->p.strategy = 4;
        z->p.depth = 1;
    }
    if (size_hint != ~(proven_u64)0 && size_hint > ((proven_u64)256 << 10) && z->opt.level >= 6 && z->opt.level <= 9) {
        z->p.hlog = LEVELS_BIG[z->opt.level - 6][0];
        z->p.clog = LEVELS_BIG[z->opt.level - 6][1];
    }
    if (z->p.hlog > need + 1) z->p.hlog = need + 1;
    if (z->p.clog > need + 1) z->p.clog = need + 1;
    z->window = (proven_size_t)1 << z->wlog;
    z->block_size = z->window < ZSTD_BLOCK_MAX ? z->window : ZSTD_BLOCK_MAX;
    z->buf_max = z->window + (z->window / 2 > z->block_size ? z->window / 2 : z->block_size) + dlen;
    z->hash_size = (proven_size_t)1 << z->p.hlog;
    z->hash2_size = z->p.strategy ? (proven_size_t)1 << z->p.clog : 0;     /* the second table, or the chain */
    proven_size_t got = 0;
    proven_err_t e = grow(z, (void **)&z->hash, &got, z->hash_size + z->hash2_size, sizeof(proven_u32));
    if (e != PROVEN_OK) { o->alloc.free_fn(o->alloc.ctx, z); return e; }
    z->hash2 = z->hash2_size ? z->hash + z->hash_size : NULL;
    for (proven_size_t i = 0; i < z->hash_size + z->hash2_size; ++i) z->hash[i] = 0;
    if (z->p.strategy == 4) {
        /* The path's nodes and the three-byte table, no larger than a known input needs. */
        const proven_u32 span = size_hint < OPT_NUM ? (proven_u32)size_hint + 1 : OPT_NUM;
        const unsigned h3_log = need + 1 < OPT_H3_LOG ? need + 1 : OPT_H3_LOG;
        const proven_size_t nodes = ((proven_size_t)span + 2) * sizeof(proven_opt_node_t), h3n = (proven_size_t)1 << h3_log;
        proven_result_mem_mut_t om = o->alloc.alloc_fn(o->alloc.ctx, sizeof(struct proven_opt) + nodes + h3n * sizeof(proven_u32), 16);
        if (om.err != PROVEN_OK) { o->alloc.free_fn(o->alloc.ctx, z->hash); o->alloc.free_fn(o->alloc.ctx, z); return PROVEN_ERR_NOMEM; }
        z->optp = (struct proven_opt *)(void *)om.value.ptr;
        z->optp->ready = false;
        z->optp->span_max = span;
        z->optp->h3_log = h3_log;
        z->optp->node = (proven_opt_node_t *)(void *)(om.value.ptr + sizeof(struct proven_opt));
        z->optp->h3 = (proven_u32 *)(void *)(om.value.ptr + sizeof(struct proven_opt) + nodes);
        for (proven_size_t i = 0; i < h3n; ++i) z->optp->h3[i] = 0;
        z->optp->h3_next = 0;
    }
    /* From level 6 the level's own matching finds about as much (a fifth of a percent at most), and
     * from 9 a long match forced into the parse costs more than it saves - 3 % at 19 (RFC-0039
     * section 7): the option is for levels 1 to 5. */
    if (o->long_distance && z->opt.level <= LDM_LEVEL_MAX && size_hint > LDM_MIN) {
        /* One entry for every 2^LDM_RATE_LOG positions of the window: 2^(window_log - 7) of them. */
        z->ldm_log = need > LDM_RATE_LOG + LDM_BUCKET_LOG + 3 ? need - LDM_RATE_LOG : LDM_BUCKET_LOG + 3;
        z->ldm_cap = (size_hint < ZSTD_BLOCK_MAX ? (proven_size_t)size_hint : ZSTD_BLOCK_MAX) / LDM_MIN + 1;
        const proven_size_t ne = (proven_size_t)1 << z->ldm_log, nb = ne >> LDM_BUCKET_LOG;
        proven_result_mem_mut_t lm = o->alloc.alloc_fn(o->alloc.ctx, ne * sizeof(proven_u64) + z->ldm_cap * sizeof(struct proven_ldm_match) + nb, 16);
        if (lm.err != PROVEN_OK) { proven_zstd_encoder_destroy(z); return PROVEN_ERR_NOMEM; }
        z->ldm = (proven_u64 *)(void *)lm.value.ptr;
        z->ldm_m = (struct proven_ldm_match *)(void *)(lm.value.ptr + ne * sizeof(proven_u64));
        z->ldm_next = lm.value.ptr + ne * sizeof(proven_u64) + z->ldm_cap * sizeof(struct proven_ldm_match);
    }
    if (dlen > 0) {
        proven_result_mem_mut_t cm = o->alloc.alloc_fn(o->alloc.ctx, sizeof(struct proven_zstd_cdict), 16);
        if (cm.err != PROVEN_OK) { proven_zstd_encoder_destroy(z); return PROVEN_ERR_NOMEM; }
        struct proven_zstd_cdict *cd = (struct proven_zstd_cdict *)(void *)cm.value.ptr;
        cd->d = dr;
        if (dr.formatted) {
            huf_from_weights(&cd->huf, dr.huf_w, dr.huf_n, dr.huf_log);
            for (int k = 0; k < 3; ++k) fse_tab_build(&cd->tab[k], dr.norm[k], dr.nsym[k], dr.log[k]);
        }
        z->cd = cd;
    }
    z->fresh = true;
    /* A block of n bytes has at most n / 4 sequences: every match is at least 4 long. */
    z->lit_max = size_hint < z->block_size ? (proven_size_t)size_hint : z->block_size;
    z->seq_max = z->lit_max / 4 + 2 < SEQ_MAX ? z->lit_max / 4 + 2 : SEQ_MAX;
    frame_start(z);
    *out = z;
    return PROVEN_OK;
}

proven_err_t proven_zstd_encoder_create(const proven_zstd_encoder_options_t *options, proven_zstd_encoder_t **out) {
    return make(options, ~(proven_u64)0, out);
}

void proven_zstd_encoder_destroy(proven_zstd_encoder_t *z) {
    if (!z) return;
    const proven_allocator_t a = z->alloc;
    if (z->hash) a.free_fn(a.ctx, z->hash);
    if (z->optp) a.free_fn(a.ctx, z->optp);
    if (z->ldm) a.free_fn(a.ctx, z->ldm);
    if (z->cd) a.free_fn(a.ctx, z->cd);
    if (z->seqs) a.free_fn(a.ctx, z->seqs);
    if (z->codes) a.free_fn(a.ctx, z->codes);
    if (z->lits) a.free_fn(a.ctx, z->lits);
    if (z->buf) a.free_fn(a.ctx, z->buf);
    if (z->pend) a.free_fn(a.ctx, z->pend);
    a.free_fn(a.ctx, z);
}

void proven_zstd_encoder_reset(proven_zstd_encoder_t *z) {
    if (z) frame_start(z);
}

/* Room for one block's sequences and literals. */
static proven_err_t block_room(proven_zstd_encoder_t *z) {
    proven_size_t cap = z->seqs ? z->seq_max : 0, ccap = z->codes ? 3 * z->seq_max : 0, lcap = z->lits ? z->lit_max + SLACK : 0;
    proven_err_t e = grow(z, (void **)&z->seqs, &cap, z->seq_max, sizeof(proven_seq_t));
    if (e == PROVEN_OK) e = grow(z, (void **)&z->codes, &ccap, 3 * z->seq_max, 1);
    if (e == PROVEN_OK) e = grow(z, (void **)&z->lits, &lcap, z->lit_max + SLACK, 1);
    return e;
}

// -----------------------------------------------------------------------------
// Matching
// -----------------------------------------------------------------------------

static inline unsigned ctz64(proven_u64 v) {
#if ZSTD_BUILTINS
    return (unsigned)__builtin_ctzll(v);
#else
    static const proven_u8 DEBRUIJN[64] = { 0, 1, 48, 2, 57, 49, 28, 3, 61, 58, 50, 42, 38, 29, 17, 4, 62, 55, 59, 36, 53, 51, 43, 22,
                                            45, 39, 33, 30, 24, 18, 12, 5, 63, 47, 56, 27, 60, 41, 37, 16, 54, 35, 52, 21, 44, 32, 23, 11,
                                            46, 26, 40, 15, 34, 20, 31, 10, 25, 14, 19, 9, 13, 8, 7, 6 };
    return DEBRUIJN[((v & (0 - v)) * 0x03F79D71B4CB0A89ull) >> 58];
#endif
}

/* How many bytes from `a` equal those from `b` (which is before it), stopping at `end`. */
ZSTD_HOT proven_size_t count_same(const proven_byte_t *a, const proven_byte_t *b, const proven_byte_t *end) {
    const proven_byte_t *const from = a;
    while (a + 8 <= end) {
        const proven_u64 d = le64(a) ^ le64(b);
        if (d) return (proven_size_t)(a - from) + (ctz64(d) >> 3);
        a += 8; b += 8;
    }
    while (a < end && *a == *b) { a++; b++; }
    return (proven_size_t)(a - from);
}

ZSTD_HOT proven_u32 hash_at(const proven_byte_t *p, unsigned log, unsigned mls) {
    static const proven_u64 PRIME[9] = { 0, 0, 0, 0, 0, 889523592379ull, 227718039650203ull, 58295818150454627ull, 0xCF1BBCDCB7A56463ull };
    if (mls == 4) return (le32(p) * 2654435761u) >> (32 - log);
    return (proven_u32)(((le64(p) << (64 - 8 * mls)) * PRIME[mls]) >> (64 - log));
}

ZSTD_HOT void copy16(proven_byte_t *d, const proven_byte_t *s) {
    st64(d, le64(s));
    st64(d + 8, le64(s + 8));
}

/* Where a matcher puts its sequences, held in locals while it runs: what it would otherwise
 * reach through the encoder the compiler must reload after every hash-table store. */
typedef struct {
    proven_seq_t *seq, *seq_end;
    proven_byte_t *lit;
    proven_u32 rep[3];
} proven_sink_t;

/* A sequence: `ll` literals from `lit`, then `ml` bytes from `off` back. The offset becomes a
 * repeat code where the decoder's repeat offsets allow one, and they change as they will. */
ZSTD_HOT void store(proven_sink_t *k, const proven_byte_t *lit, const proven_byte_t *lit_end, proven_size_t ll, proven_u32 off, proven_size_t ml) {
    if (lit + ll + 16 <= lit_end) {
        /* Sixteen at a time, past the end of the run: `lits` has the slack, the input the bytes. */
        proven_byte_t *d = k->lit;
        const proven_byte_t *src = lit;
        copy16(d, src);
        for (proven_size_t i = 16; i < ll; i += 16) copy16(d + i, src + i);
    } else {
        (void)proven_mem_copy(k->lit, ll, (proven_mem_view_t){ .ptr = lit, .size = ll });
    }
    k->lit += ll;
    proven_u32 *r = k->rep, v;
    if (ll > 0) {
        if (off == r[0]) v = 1;
        else if (off == r[1]) { v = 2; r[1] = r[0]; r[0] = off; }
        else if (off == r[2]) { v = 3; r[2] = r[1]; r[1] = r[0]; r[0] = off; }
        else { v = off + 3; r[2] = r[1]; r[1] = r[0]; r[0] = off; }
    } else {
        /* With no literals the codes shift by one: the first repeat offset cannot be named. */
        if (off == r[1]) { v = 1; r[1] = r[0]; r[0] = off; }
        else if (off == r[2]) { v = 2; r[2] = r[1]; r[1] = r[0]; r[0] = off; }
        else if (off == r[0] - 1) { v = 3; r[2] = r[1]; r[1] = r[0]; r[0] = off; }
        else { v = off + 3; r[2] = r[1]; r[1] = r[0]; r[0] = off; }
    }
    *k->seq++ = (proven_seq_t){ v, (proven_u32)ll, (proven_u32)ml };
}

static proven_sink_t sink_open(proven_zstd_encoder_t *z) {
    return (proven_sink_t){ z->seqs + z->nseq, z->seqs + z->seq_max, z->lits + z->nlit, { z->rep[0], z->rep[1], z->rep[2] } };
}

static void sink_close(proven_zstd_encoder_t *z, const proven_sink_t *k) {
    z->nseq = (proven_size_t)(k->seq - z->seqs);
    z->nlit = (proven_size_t)(k->lit - z->lits);
    z->rep[0] = k->rep[0]; z->rep[1] = k->rep[1]; z->rep[2] = k->rep[2];
}

/* One hash table. `base` + [start, end) is the block, `low` the first position a match may
 * begin at. Returns where the block ends: `end`, or sooner when the sequences ran out. What
 * follows the last match up to there is the block's last literals, not yet stored.
 *
 * Positions are looked at in pairs, the hash of the next computed while the current is
 * checked, and the first repeat offset is tried a step ahead; after 128 bytes without a match
 * the step grows by one (the reference encoder's fast loop). */
ZSTD_HOT proven_size_t match_fast_mls(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end,
                                      proven_size_t low, proven_size_t *last_from, const unsigned mls) {
    const unsigned hlog = z->p.hlog;
    proven_u32 *const ht = z->hash;
    const proven_byte_t *const iend = base + end, *const lowp = base + low;
    const proven_byte_t *const ilimit = end - start > 8 ? iend - 8 : base + start;
    proven_sink_t k = sink_open(z);
    const proven_byte_t *ip0 = base + start, *anchor = ip0;
    if (ip0 == base) ip0++;
    while (k.seq_end - k.seq >= 2) {
        proven_size_t step = 2;
        const proven_byte_t *next_step = ip0 + 128;
        const proven_byte_t *ip1 = ip0 + 1, *ip2 = ip0 + step, *ip3 = ip2 + 1;
        if (ip3 >= ilimit) break;
        proven_u32 h0 = hash_at(ip0, hlog, mls), h1 = hash_at(ip1, hlog, mls), cand = ht[h0], cur0, off;
        const proven_byte_t *m0;
        proven_size_t ml;
        for (;;) {
            const proven_u32 r = k.rep[0];
            cur0 = (proven_u32)(ip0 - base);
            ht[h0] = cur0;
            if (r <= (proven_size_t)(ip2 - lowp) && le32(ip2) == le32(ip2 - r)) {
                ip0 = ip2;
                m0 = ip0 - r;
                ml = m0 > lowp && ip0[-1] == m0[-1];
                ip0 -= ml; m0 -= ml; ml += 4;
                off = r;
                ht[h1] = (proven_u32)(ip1 - base);
                goto matched;
            }
            if (cur0 - cand - 1 < cur0 - low && le32(base + cand) == le32(ip0)) {
                ht[h1] = (proven_u32)(ip1 - base);
                goto found;
            }
            cand = ht[h1];
            h0 = h1; h1 = hash_at(ip2, hlog, mls);
            ip0 = ip1; ip1 = ip2; ip2 = ip3;
            cur0 = (proven_u32)(ip0 - base);
            ht[h0] = cur0;
            if (cur0 - cand - 1 < cur0 - low && le32(base + cand) == le32(ip0)) {
                if (step <= 4) ht[h1] = (proven_u32)(ip1 - base);
                goto found;
            }
            cand = ht[h1];
            h0 = h1; h1 = hash_at(ip2, hlog, mls);
            ip0 = ip1; ip1 = ip2; ip2 = ip0 + step; ip3 = ip1 + step;
            if (ip2 >= next_step) { step++; next_step += 128; }
            if (ip3 >= ilimit) goto out;
        }
    found:
        m0 = base + cand;
        off = (proven_u32)(ip0 - m0);
        ml = 4;
        while (ip0 > anchor && m0 > lowp && ip0[-1] == m0[-1]) { ip0--; m0--; ml++; }
    matched:
        ml += count_same(ip0 + ml, m0 + ml, iend);
        store(&k, anchor, iend, (proven_size_t)(ip0 - anchor), off, ml);
        ip0 += ml;
        anchor = ip0;
        if (ip0 <= ilimit) {
            ht[hash_at(base + cur0 + 2, hlog, mls)] = cur0 + 2;
            ht[hash_at(ip0 - 2, hlog, mls)] = (proven_u32)(ip0 - 2 - base);
            /* The second repeat offset straight after a match, with no literals between. */
            while (ip0 <= ilimit && k.seq_end - k.seq >= 1) {
                const proven_u32 r1 = k.rep[1];
                if (r1 > (proven_size_t)(ip0 - lowp) || le32(ip0) != le32(ip0 - r1)) break;
                const proven_size_t rl = count_same(ip0 + 4, ip0 + 4 - r1, iend) + 4;
                ht[hash_at(ip0, hlog, mls)] = (proven_u32)(ip0 - base);
                store(&k, anchor, iend, 0, r1, rl);
                ip0 += rl;
                anchor = ip0;
            }
        }
    }
out:
    *last_from = (proven_size_t)(anchor - base);
    const bool full = k.seq_end - k.seq < 2;
    sink_close(z, &k);
    return full ? (proven_size_t)(anchor - base) : end;
}

/* Two hash tables: 8-byte prefixes, tried first, and `mls`-byte ones. */
ZSTD_HOT proven_size_t match_double_mls(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end,
                                             proven_size_t low, proven_size_t *last_from, const unsigned mls) {
    const unsigned hlog = z->p.hlog, clog = z->p.clog;
    proven_u32 *const hl = z->hash, *const hs = z->hash2;
    const proven_byte_t *const iend = base + end, *const lowp = base + low;
    const proven_byte_t *const ilimit = end - start > 8 ? iend - 8 : base + start;
    proven_sink_t k = sink_open(z);
    const proven_byte_t *ip = base + start, *anchor = ip;
    if (ip == base) ip++;
    while (ip < ilimit) {
        if (k.seq_end - k.seq < 2) break;
        const proven_u32 h8 = hash_at(ip, hlog, 8), h = hash_at(ip, clog, mls), cur = (proven_u32)(ip - base);
        const proven_u32 cl = hl[h8], cs = hs[h];
        hl[h8] = hs[h] = cur;
        const proven_u32 r0 = k.rep[0];
        proven_size_t ml;
        proven_u32 off;
        if (r0 <= cur + 1 - low && le32(ip + 1 - r0) == le32(ip + 1)) {
            ml = count_same(ip + 5, ip + 5 - r0, iend) + 4;
            ip++;
            store(&k, anchor, iend, (proven_size_t)(ip - anchor), r0, ml);
            goto matched;
        }
        if (cl >= low && cl < cur && le64(base + cl) == le64(ip)) {
            const proven_byte_t *m = base + cl;
            ml = count_same(ip + 8, m + 8, iend) + 8;
            off = (proven_u32)(ip - m);
            while (ip > anchor && m > lowp && ip[-1] == m[-1]) { ip--; m--; ml++; }
        } else if (cs >= low && cs < cur && le32(base + cs) == le32(ip)) {
            /* A short match: perhaps a long one begins at the next byte. */
            const proven_u32 h8n = hash_at(ip + 1, hlog, 8), cn = hl[h8n];
            hl[h8n] = cur + 1;
            const proven_byte_t *m;
            if (cn >= low && cn < cur + 1 && le64(base + cn) == le64(ip + 1)) {
                m = base + cn;
                ml = count_same(ip + 9, m + 8, iend) + 8;
                ip++;
            } else {
                m = base + cs;
                ml = count_same(ip + 4, m + 4, iend) + 4;
            }
            off = (proven_u32)(ip - m);
            while (ip > anchor && m > lowp && ip[-1] == m[-1]) { ip--; m--; ml++; }
        } else {
            ip += ((ip - anchor) >> 8) + 1;
            continue;
        }
        store(&k, anchor, iend, (proven_size_t)(ip - anchor), off, ml);
    matched:
        ip += ml;
        anchor = ip;
        if (ip <= ilimit) {
            const proven_u32 i2 = cur + 2;
            hl[hash_at(base + i2, hlog, 8)] = i2;
            hs[hash_at(base + i2, clog, mls)] = i2;
            hl[hash_at(ip - 2, hlog, 8)] = (proven_u32)(ip - 2 - base);
            hs[hash_at(ip - 1, clog, mls)] = (proven_u32)(ip - 1 - base);
            while (ip <= ilimit && k.seq_end - k.seq >= 1) {
                const proven_u32 r1 = k.rep[1];
                if (r1 > (proven_size_t)(ip - lowp) || le32(ip) != le32(ip - r1)) break;
                const proven_size_t rl = count_same(ip + 4, ip + 4 - r1, iend) + 4;
                hs[hash_at(ip, clog, mls)] = hl[hash_at(ip, hlog, 8)] = (proven_u32)(ip - base);
                store(&k, anchor, iend, 0, r1, rl);
                ip += rl;
                anchor = ip;
            }
        }
    }
    *last_from = (proven_size_t)(anchor - base);
    const bool full = k.seq_end - k.seq < 2;
    sink_close(z, &k);
    return full ? (proven_size_t)(anchor - base) : end;
}

/* Put every position from `next_update` up to `target` into the hash table, each linked to the
 * one its hash held before: the chain, indexed by position modulo its size. */
ZSTD_HOT void hc_insert(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_u32 target, const unsigned mls) {
    proven_u32 *const ht = z->hash, *const ch = z->hash2;
    const proven_u32 mask = (proven_u32)z->hash2_size - 1;
    const unsigned hlog = z->p.hlog;
    for (proven_u32 i = z->next_update; i < target; ++i) {
        const proven_u32 h = hash_at(base + i, hlog, mls);
        ch[i & mask] = ht[h];
        ht[h] = i;
    }
    if (target > z->next_update) z->next_update = target;
}

/* The longest match for `ip` among the first positions of its chain, at least 4 long;
 * 0 when there is none, looking at `tries` candidates. Older positions than the chain holds share slots with newer ones: the
 * walk stops before them. Every candidate is compared byte by byte. */
ZSTD_HOT proven_size_t hc_find(proven_zstd_encoder_t *z, const proven_byte_t *base, const proven_byte_t *ip, const proven_byte_t *iend,
                               proven_size_t low, proven_u32 *off, const unsigned mls) {
    const proven_u32 cur = (proven_u32)(ip - base), mask = (proven_u32)z->hash2_size - 1;
    hc_insert(z, base, cur, mls);
    const proven_u32 *const ch = z->hash2;
    const proven_u32 min_chain = cur > mask + 1 ? cur - (mask + 1) : 0;
    proven_u32 idx = z->hash[hash_at(ip, z->p.hlog, mls)];
    proven_size_t best = 3;
    const proven_size_t room = (proven_size_t)(iend - ip);
    for (unsigned n = z->p.tries; n > 0 && idx >= low && idx < cur; --n) {
        const proven_byte_t *m = base + idx;
        if (best < room && m[best] == ip[best]) {
            const proven_size_t len = count_same(ip, m, iend);
            if (len > best) {
                best = len;
                *off = cur - idx;
                if (len == room) break;
            }
        }
        if (idx <= min_chain) break;
        idx = ch[idx & mask];
    }
    return best >= 4 ? best : 0;
}

/* Binary trees (levels 13 to 15): each position is a node of the tree its hash heads, ordered by the
 * bytes that follow it; its two links - to the nodes before it that sort below it and above it - are
 * hash2[2 (p mod n)] and hash2[2 (p mod n) + 1], n = hash2_size / 2, 0 for none. A node older than
 * cur - n shares its slots with a newer one: a walk links it and goes no further. Putting `cur` in
 * walks down from the head as a search does, linking `cur` in on the way, and finds the longest match
 * among the nodes compared (at most `tries`); at least 4 long, or 0. */
ZSTD_HOT proven_size_t bt_put(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_u32 cur, const proven_byte_t *iend,
                              proven_size_t low, proven_u32 *off, const unsigned mls) {
    proven_u32 *const ht = z->hash, *const bt = z->hash2;
    const proven_u32 nmask = (proven_u32)(z->hash2_size / 2) - 1;
    const proven_byte_t *const ip = base + cur;
    const proven_u32 h = hash_at(ip, z->p.hlog, mls);
    proven_u32 idx = ht[h];
    ht[h] = cur;
    proven_u32 dummy = 0;
    proven_u32 *smaller = bt + 2 * (cur & nmask), *larger = smaller + 1;
    proven_size_t lsmall = 0, llarge = 0, best = 0;
    const proven_u32 old = cur > nmask ? cur - nmask : 0;
    for (unsigned n = z->p.tries; n > 0 && idx > 0 && idx >= low; --n) {
        proven_u32 *const next = bt + 2 * (idx & nmask);
        const proven_byte_t *const m = base + idx;
        proven_size_t len = lsmall < llarge ? lsmall : llarge;
        len += count_same(ip + len, m + len, iend);
        if (len > best) { best = len; *off = cur - idx; }
        if (ip + len == iend) break;                  /* the input ends: which side is not known */
        if (m[len] < ip[len]) {
            *smaller = idx;
            lsmall = len;
            if (idx <= old) { smaller = &dummy; break; }
            smaller = next + 1;
            idx = next[1];
        } else {
            *larger = idx;
            llarge = len;
            if (idx <= old) { larger = &dummy; break; }
            larger = next;
            idx = next[0];
        }
    }
    *smaller = 0;
    *larger = 0;
    return best >= 4 ? best : 0;
}

/* Put the positions since the last search into the trees. One that finds a match longer than 384
 * bytes lets the next ones, up to 192, go without: they would find the same match a byte shorter, at
 * the cost of a walk each - in a run of one byte, a walk the length of the block, and the block's
 * square in all (libzstd's rule too). */
ZSTD_HOT void bt_update(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_u32 cur, const proven_byte_t *iend, proven_size_t low, const unsigned mls) {
    proven_u32 skip = 0;
    for (proven_u32 i = z->next_update; i < cur;) {
        const proven_size_t got = bt_put(z, base, i, iend, low, &skip, mls);
        i += got > 384 ? (proven_u32)(got - 384 < 192 ? got - 384 : 192) : 1u;
    }
}

/* The longest match for `ip` by the trees: the positions since the last search put in first. */
ZSTD_HOT proven_size_t bt_find(proven_zstd_encoder_t *z, const proven_byte_t *base, const proven_byte_t *ip, const proven_byte_t *iend,
                               proven_size_t low, proven_u32 *off, const unsigned mls) {
    const proven_u32 cur = (proven_u32)(ip - base);
    bt_update(z, base, cur, iend, low, mls);
    z->next_update = cur + 1;
    return bt_put(z, base, cur, iend, low, off, mls);
}

/* ---- Optimal parsing (levels 16 to 19, RFC-0038 Z-3) ----
 *
 * Every match the trees find at a position, and the three repeat offsets, each weighed by what it
 * would cost to write - its offset code and extra bits, its length's, the literal length's of its
 * sequence - and every literal by its byte; the cheapest path through a stretch of input (up to
 * OPT_NUM positions, or a match of `target` bytes) is taken. The prices come from what the frame has
 * written so far (each block starts from the last's, scaled down; the first from its own bytes and
 * the format's predefined distributions), as libzstd's btopt does - written here from the idea, not
 * from its code. */

ZSTD_HOT unsigned ll_code(proven_u32 ll);
ZSTD_HOT unsigned ml_code(proven_u32 ml);

/* About 256 log2(stat + 1): the whole bits from the highest set bit, the fraction from the next. */
static inline proven_u32 opt_weight(proven_u32 stat) {
    const proven_u32 s = stat + 1, hb = highbit32(s);
    return hb * 256 + ((s << 8) >> hb);
}

/* Each symbol's price from the counts. */
static void opt_prices(struct proven_opt *o) {
    o->lit_w = opt_weight(o->lit_sum); o->ll_w = opt_weight(o->ll_sum); o->ml_w = opt_weight(o->ml_sum); o->of_w = opt_weight(o->of_sum);
    for (unsigned i = 0; i < 256; ++i) { const proven_u32 w = opt_weight(o->lit[i]); o->lit_p[i] = o->lit_w > w ? o->lit_w - w : 0; }
    for (unsigned i = 0; i < 36; ++i) { const proven_u32 w = opt_weight(o->ll[i]); o->ll_p[i] = LL_BITS[i] * 256u + (o->ll_w > w ? o->ll_w - w : 0); }
    for (unsigned i = 0; i < 53; ++i) { const proven_u32 w = opt_weight(o->ml[i]); o->ml_p[i] = ML_BITS[i] * 256u + (o->ml_w > w ? o->ml_w - w : 0); }
    for (unsigned i = 0; i < 32; ++i) { const proven_u32 w = opt_weight(o->of[i]); o->of_p[i] = i * 256u + (o->of_w > w ? o->of_w - w : 0); }
}

static proven_u32 opt_sum(const proven_u32 *t, unsigned n) { proven_u32 s = 0; for (unsigned i = 0; i < n; ++i) s += t[i]; return s; }

/* A block begins: its counts are the last block's scaled down, or - first in the frame - its own
 * bytes' and the predefined distributions'. */
static void opt_begin(struct proven_opt *o, const proven_byte_t *src, proven_size_t n) {
    if (!o->ready) {
        for (unsigned i = 0; i < 256; ++i) o->lit[i] = 0;
        for (proven_size_t i = 0; i < n; ++i) o->lit[src[i]]++;
        for (unsigned i = 0; i < 256; ++i) o->lit[i] = 1 + (o->lit[i] >> 4);
        for (unsigned i = 0; i < 36; ++i) o->ll[i] = LL_DEFAULT[i] > 0 ? (proven_u32)LL_DEFAULT[i] : 1;
        for (unsigned i = 0; i < 53; ++i) o->ml[i] = ML_DEFAULT[i] > 0 ? (proven_u32)ML_DEFAULT[i] : 1;
        for (unsigned i = 0; i < 32; ++i) o->of[i] = i < 29 && OF_DEFAULT[i] > 0 ? (proven_u32)OF_DEFAULT[i] : 1;
        o->ready = true;
    } else {
        for (unsigned i = 0; i < 256; ++i) o->lit[i] = 1 + (o->lit[i] >> 4);
        for (unsigned i = 0; i < 36; ++i) o->ll[i] = 1 + (o->ll[i] >> 4);
        for (unsigned i = 0; i < 53; ++i) o->ml[i] = 1 + (o->ml[i] >> 4);
        for (unsigned i = 0; i < 32; ++i) o->of[i] = 1 + (o->of[i] >> 4);
    }
    o->lit_sum = opt_sum(o->lit, 256); o->ll_sum = opt_sum(o->ll, 36); o->ml_sum = opt_sum(o->ml, 53); o->of_sum = opt_sum(o->of, 32);
    opt_prices(o);
}

static inline proven_u32 opt_ll_price(const struct proven_opt *o, proven_u32 ll) { return o->ll_p[ll_code(ll)]; }
static inline proven_u32 opt_match_price(const struct proven_opt *o, proven_u32 offv, proven_u32 ml) { return o->of_p[highbit32(offv)] + o->ml_p[ml_code(ml)]; }

/* The repeat offsets after a sequence with `ll` literals and a match `dist` back, and the offset
 * value written for it - as store() makes them. */
static inline proven_u32 opt_new_rep(proven_u32 r[3], proven_u32 dist, bool ll0) {
    proven_u32 v;
    if (!ll0) {
        if (dist == r[0]) return 1;
        if (dist == r[1]) { v = 2; r[1] = r[0]; r[0] = dist; return v; }
        if (dist == r[2]) { v = 3; r[2] = r[1]; r[1] = r[0]; r[0] = dist; return v; }
    } else {
        if (dist == r[1]) { v = 1; r[1] = r[0]; r[0] = dist; return v; }
        if (dist == r[2]) { v = 2; r[2] = r[1]; r[1] = r[0]; r[0] = dist; return v; }
        if (dist == r[0] - 1) { v = 3; r[2] = r[1]; r[1] = r[0]; r[0] = dist; return v; }
    }
    r[2] = r[1]; r[1] = r[0]; r[0] = dist;
    return dist + 3;
}

/* Every match at `cur` longer than the last found, shortest first: the repeat offsets (as the
 * sequence's literal count would name them), then the trees' - which put `cur` in on the way. */
ZSTD_HOT unsigned opt_find(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_u32 cur, const proven_byte_t *iend, proven_size_t low,
                           const proven_u32 rep[3], bool ll0, proven_opt_match_t *m) {
    const proven_byte_t *const ip = base + cur;
    unsigned n = 0;
    const bool three = z->p.depth >= 1;
    proven_size_t best = three ? 2 : 3;
    for (unsigned i = 0; i < 3; ++i) {
        const proven_u32 d = ll0 ? (i == 0 ? rep[1] : i == 1 ? rep[2] : rep[0] - 1) : rep[i];
        if (d == 0 || d > cur - low) continue;
        const proven_byte_t *const mp = ip - d;
        if (ip[0] != mp[0] || ip[1] != mp[1] || ip[2] != mp[2]) continue;
        const proven_size_t len = count_same(ip + 3, mp + 3, iend) + 3;
        if (len > best) { m[n++] = (proven_opt_match_t){ i + 1, d, (proven_u32)len }; best = len; }
    }
    if (three) {
        /* The nearest earlier position that begins with the same three bytes, if near enough. */
        struct proven_opt *const o = z->optp;
        for (proven_u32 i = o->h3_next; i < cur; ++i) o->h3[(((proven_u32)base[i] | (proven_u32)base[i + 1] << 8 | (proven_u32)base[i + 2] << 16) * 506832829u) >> (32 - o->h3_log)] = i;
        const proven_u32 h3 = (((proven_u32)ip[0] | (proven_u32)ip[1] << 8 | (proven_u32)ip[2] << 16) * 506832829u) >> (32 - o->h3_log);
        const proven_u32 c3 = o->h3[h3];
        if (cur >= o->h3_next) { o->h3[h3] = cur; o->h3_next = cur + 1; }
        if (c3 > 0 && c3 >= low && c3 < cur && cur - c3 < OPT_H3_REACH) {
            const proven_size_t len = count_same(ip, base + c3, iend);
            if (len > best) { m[n++] = (proven_opt_match_t){ cur - c3 + 3, cur - c3, (proven_u32)len }; best = len; }
        }
    }
    /* The trees: positions skipped since the last search go in first. A position already in them
     * (a stretch ended before the last one searched) is not put in again: its repeats only. */
    if (cur < z->next_update) return n;
    bt_update(z, base, cur, iend, low, z->p.mls);
    z->next_update = cur + 1;
    proven_u32 *const ht = z->hash, *const bt = z->hash2;
    const proven_u32 nmask = (proven_u32)(z->hash2_size / 2) - 1;
    const proven_u32 h = hash_at(ip, z->p.hlog, z->p.mls);
    proven_u32 idx = ht[h];
    ht[h] = cur;
    proven_u32 dummy = 0;
    proven_u32 *smaller = bt + 2 * (cur & nmask), *larger = smaller + 1;
    proven_size_t lsmall = 0, llarge = 0;
    const proven_u32 old = cur > nmask ? cur - nmask : 0;
    for (unsigned k = z->p.tries; k > 0 && idx > 0 && idx >= low; --k) {
        proven_u32 *const next = bt + 2 * (idx & nmask);
        const proven_byte_t *const mp = base + idx;
        proven_size_t len = lsmall < llarge ? lsmall : llarge;
        len += count_same(ip + len, mp + len, iend);
        if (len > best && n < OPT_MATCHES) { m[n++] = (proven_opt_match_t){ cur - idx + 3, cur - idx, (proven_u32)len }; best = len; }
        if (ip + len == iend) break;
        if (mp[len] < ip[len]) {
            *smaller = idx; lsmall = len;
            if (idx <= old) { smaller = &dummy; break; }
            smaller = next + 1; idx = next[1];
        } else {
            *larger = idx; llarge = len;
            if (idx <= old) { larger = &dummy; break; }
            larger = next; idx = next[0];
        }
    }
    *smaller = 0;
    *larger = 0;
    return n;
}

/* A sequence written, and counted. */
static void opt_store(struct proven_opt *o, proven_sink_t *k, const proven_byte_t *lit, const proven_byte_t *iend, proven_size_t ll, proven_u32 dist, proven_size_t ml) {
    proven_u32 r[3] = { k->rep[0], k->rep[1], k->rep[2] };
    const proven_u32 offv = opt_new_rep(r, dist, ll == 0);
    for (proven_size_t i = 0; i < ll; ++i) o->lit[lit[i]] += 2;
    o->lit_sum += 2 * (proven_u32)ll;
    o->ll[ll_code((proven_u32)ll)]++; o->ll_sum++;
    o->ml[ml_code((proven_u32)ml)]++; o->ml_sum++;
    o->of[highbit32(offv)]++; o->of_sum++;
    store(k, lit, iend, ll, dist, ml);
}

ZSTD_HOT proven_size_t match_opt(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end,
                                 proven_size_t low, proven_size_t *last_from) {
    struct proven_opt *const o = z->optp;
    const proven_byte_t *const iend = base + end;
    const proven_byte_t *const ilimit = end - start > 8 ? iend - 8 : base + start;
    proven_sink_t k = sink_open(z);
    const proven_byte_t *ip = base + start, *anchor = ip;
    if (ip == base) ip++;
    opt_begin(o, base + start, end - start);
    proven_opt_node_t *const nd = o->node;
    while (ip < ilimit && k.seq_end - k.seq >= 3) {
        /* A stretch writes at most one sequence for every match's shortest length (three or four
         * bytes), and one more: no longer than the sequences left allow. */
        const proven_size_t room = (proven_size_t)(k.seq_end - k.seq) - 2, shortest = z->p.depth >= 1 ? 3 : 4;
        const proven_u32 span = room * shortest < o->span_max ? (proven_u32)(room * shortest) : o->span_max;
        const proven_u32 cur0 = (proven_u32)(ip - base);
        const proven_u32 ll0 = (proven_u32)(ip - anchor);
        unsigned nm = opt_find(z, base, cur0, iend, low, k.rep, ll0 == 0, o->m);
        if (nm == 0) { ip++; continue; }
        if (o->m[nm - 1].len >= z->p.target) {
            /* Long enough to take at once. */
            const proven_opt_match_t mm = o->m[nm - 1];
            opt_store(o, &k, anchor, iend, ll0, mm.dist, mm.len);
            ip = anchor = ip + mm.len;
            opt_prices(o);
            continue;
        }
        /* The first node: the literals already waiting. Its price holds their length's price; a
         * node at a match's end holds the whole sequence and not the next one's length. */
        nd[0] = (proven_opt_node_t){ .price = ll0 ? opt_ll_price(o, ll0) : 0, .litlen = ll0, .rep = { k.rep[0], k.rep[1], k.rep[2] } };
        proven_u32 last = 0;
        proven_u32 far_dist = 0, far_len = 0, far_at = 0;   /* a match past the stretch, taken whole at its end */
        for (proven_u32 cur = 0;; ++cur) {
            if (cur > 0) {
                /* A literal into `cur`, if cheaper. */
                const proven_opt_node_t *pv = &nd[cur - 1];
                const proven_u32 lp = pv->litlen;
                const proven_u32 price = pv->price + o->lit_p[ip[cur - 1]] + opt_ll_price(o, lp + 1) - (lp ? opt_ll_price(o, lp) : 0);
                if (cur > last || price <= nd[cur].price) {
                    nd[cur] = (proven_opt_node_t){ .price = price, .litlen = lp + 1, .rep = { pv->rep[0], pv->rep[1], pv->rep[2] } };
                    if (cur > last) last = cur;
                } else if (nd[cur].mlen > 0) {
                    /* A match ends here: the repeat offsets after it. */
                    const proven_opt_node_t *st = &nd[cur - nd[cur].mlen];
                    proven_u32 r[3] = { st->rep[0], st->rep[1], st->rep[2] };
                    (void)opt_new_rep(r, nd[cur].dist, st->litlen == 0);
                    nd[cur].rep[0] = r[0]; nd[cur].rep[1] = r[1]; nd[cur].rep[2] = r[2];
                }
                if (cur == last) break;
                if (ip + cur >= ilimit) continue;
                nm = opt_find(z, base, cur0 + cur, iend, low, nd[cur].rep, nd[cur].litlen == 0, o->m);
                if (nm == 0) continue;
            }
            const proven_opt_node_t *at = &nd[cur];
            const proven_u32 longest = o->m[nm - 1].len;
            if (longest >= z->p.target || cur + longest > span) {
                /* Long enough, or past the stretch: the path ends here and the match follows it. */
                far_dist = o->m[nm - 1].dist; far_len = longest; far_at = cur;
                last = cur;
                break;
            }
            const proven_u32 base_price = at->price + (at->litlen ? 0 : opt_ll_price(o, 0));
            proven_u32 len = z->p.depth >= 1 ? 3 : 4;
            for (unsigned i = 0; i < nm; ++i) {
                const proven_opt_match_t mm = o->m[i];
                for (; len <= mm.len; ++len) {
                    const proven_u32 pos = cur + len;
                    const proven_u32 price = base_price + opt_match_price(o, mm.offv, len);
                    if (pos > last) { for (proven_u32 q = last + 1; q < pos; ++q) nd[q].price = 0xffffffffu, nd[q].mlen = 0, nd[q].litlen = 0; last = pos; nd[pos].price = 0xffffffffu; }
                    if (price < nd[pos].price) {
                        nd[pos] = (proven_opt_node_t){ .price = price, .dist = mm.dist, .mlen = len, .litlen = 0 };
                        /* rep set when the walk reaches pos */
                        const proven_opt_node_t *st = at;
                        proven_u32 r[3] = { st->rep[0], st->rep[1], st->rep[2] };
                        (void)opt_new_rep(r, mm.dist, st->litlen == 0);
                        nd[pos].rep[0] = r[0]; nd[pos].rep[1] = r[1]; nd[pos].rep[2] = r[2];
                    }
                }
            }
        }
        /* Back from the end of the path's last match: its matches, in order. */
        proven_u32 end_at = last;
        while (end_at > 0 && nd[end_at].mlen == 0) end_at--;
        proven_u32 stack[OPT_NUM / 3 + 2], ns = 0;          /* a match is three bytes or more */
        for (proven_u32 pos = end_at; pos > 0;) {
            if (nd[pos].mlen > 0) { stack[ns++] = pos; pos -= nd[pos].mlen; }
            else pos--;
        }
        while (ns > 0) {
            const proven_u32 pos = stack[--ns];
            const proven_byte_t *const ms = ip + pos - nd[pos].mlen;
            opt_store(o, &k, anchor, iend, (proven_size_t)(ms - anchor), nd[pos].dist, nd[pos].mlen);
            anchor = ms + nd[pos].mlen;
        }
        if (far_len > 0) {
            /* The match the path stopped at, after the path's matches (it starts at or after their end). */
            const proven_byte_t *const ms = ip + far_at;
            opt_store(o, &k, anchor, iend, (proven_size_t)(ms - anchor), far_dist, far_len);
            anchor = ms + far_len;
        }
        ip = anchor > ip ? anchor : ip + 1;
        opt_prices(o);
    }
    *last_from = (proven_size_t)(anchor - base);
    const bool full = k.seq_end - k.seq < 3;
    sink_close(z, &k);
    return full ? (proven_size_t)(anchor - base) : end;
}

/* The cost of an offset in the reference encoder's comparisons: the bits of its offset value. */
static inline int off_bits(proven_u32 off, bool rep) { return rep ? 0 : (int)highbit32(off + 3); }

/* A hash chain, with a decision put off by `depth` positions: greedy (0) takes the first match
 * found; lazy (1) and lazy2 (2) look one and two bytes on for a better one, weighing length
 * against the offset's bits (the reference encoder's lazy loop). */
ZSTD_HOT proven_size_t match_lazy_mls(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end,
                                      proven_size_t low, proven_size_t *last_from, const unsigned mls, const unsigned depth, const bool tree) {
    const proven_byte_t *const iend = base + end, *const lowp = base + low;
    const proven_byte_t *const ilimit = end - start > 8 ? iend - 8 : base + start;
    proven_sink_t k = sink_open(z);
    const proven_byte_t *ip = base + start, *anchor = ip;
    if (ip == base) ip++;
    while (ip < ilimit && k.seq_end - k.seq >= 2) {
        proven_size_t ml = 0;
        proven_u32 off = 0;
        bool rep = false;
        const proven_byte_t *st = ip + 1;
        const proven_u32 r0 = k.rep[0];
        if (r0 <= (proven_size_t)(ip + 1 - lowp) && le32(ip + 1) == le32(ip + 1 - r0)) {
            ml = count_same(ip + 5, ip + 5 - r0, iend) + 4;
            off = r0;
            rep = true;
            if (depth == 0) goto store_it;
        }
        {
            proven_u32 o2 = 0;
            const proven_size_t m2 = tree ? bt_find(z, base, ip, iend, low, &o2, mls) : hc_find(z, base, ip, iend, low, &o2, mls);
            if (m2 > ml) { ml = m2; off = o2; rep = false; st = ip; }
        }
        if (ml < 4) {
            ip += ((ip - anchor) >> 8) + 1;
            continue;
        }
        for (unsigned d = 1; d <= depth && ip < ilimit; ++d) {
            ip++;
            const int extra = d == 1 ? 4 : 7;
            if (!rep && r0 <= (proven_size_t)(ip - lowp) && le32(ip) == le32(ip - r0)) {
                const proven_size_t mr = count_same(ip + 4, ip + 4 - r0, iend) + 4;
                const int g2 = (int)mr * (d == 1 ? 3 : 4), g1 = (int)ml * (d == 1 ? 3 : 4) - off_bits(off, rep) + 1;
                if (g2 > g1) { ml = mr; off = r0; rep = true; st = ip; }
            }
            proven_u32 o2 = 0;
            const proven_size_t m2 = tree ? bt_find(z, base, ip, iend, low, &o2, mls) : hc_find(z, base, ip, iend, low, &o2, mls);
            if (m2 >= 4) {
                const int g2 = (int)m2 * 4 - off_bits(o2, false), g1 = (int)ml * 4 - off_bits(off, rep) + extra;
                if (g2 > g1) { ml = m2; off = o2; rep = false; st = ip; d = 0; continue; }   /* a better match: look on from it */
            }
        }
        if (!rep) while (st > anchor && (proven_size_t)(st - lowp) > off && st[-1] == *(st - 1 - (proven_size_t)off)) { st--; ml++; }
    store_it:
        store(&k, anchor, iend, (proven_size_t)(st - anchor), off, ml);
        ip = anchor = st + ml;
        while (ip <= ilimit && k.seq_end - k.seq >= 1) {
            const proven_u32 r1 = k.rep[1];
            if (r1 > (proven_size_t)(ip - lowp) || le32(ip) != le32(ip - r1)) break;
            const proven_size_t rl = count_same(ip + 4, ip + 4 - r1, iend) + 4;
            store(&k, anchor, iend, 0, r1, rl);
            ip += rl;
            anchor = ip;
        }
    }
    *last_from = (proven_size_t)(anchor - base);
    const bool full = k.seq_end - k.seq < 2;
    sink_close(z, &k);
    return full ? (proven_size_t)(anchor - base) : end;
}

ZSTD_HOT proven_size_t match_block(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end, proven_size_t low, proven_size_t *last_from) {
    z->fresh = false;
    if (z->p.strategy == 0) {
        return match_fast_mls(z, base, start, end, low, last_from, 6);
    }
    if (z->p.strategy == 2) {
        const unsigned m4 = z->p.mls == 4;
        switch (z->p.depth) {
            case 0: return m4 ? match_lazy_mls(z, base, start, end, low, last_from, 4, 0, false) : match_lazy_mls(z, base, start, end, low, last_from, 5, 0, false);
            case 1: return m4 ? match_lazy_mls(z, base, start, end, low, last_from, 4, 1, false) : match_lazy_mls(z, base, start, end, low, last_from, 5, 1, false);
            default: return m4 ? match_lazy_mls(z, base, start, end, low, last_from, 4, 2, false) : match_lazy_mls(z, base, start, end, low, last_from, 5, 2, false);
        }
    }
    if (z->p.strategy == 4) return match_opt(z, base, start, end, low, last_from);
    if (z->p.strategy == 3) {
        return z->p.mls == 4 ? match_lazy_mls(z, base, start, end, low, last_from, 4, 2, true) : match_lazy_mls(z, base, start, end, low, last_from, 5, 2, true);
    }
    return z->p.mls == 5 ? match_double_mls(z, base, start, end, low, last_from, 5) : match_double_mls(z, base, start, end, low, last_from, 6);
}

// -----------------------------------------------------------------------------
// A block's entropy coding
// -----------------------------------------------------------------------------

static const proven_u8 LL_CODE[64] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21,
                                       22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24 };
static const proven_u8 ML_CODE[128] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                                        32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 36, 36, 37, 37, 37, 37, 38, 38, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 39, 39,
                                        40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
                                        42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42 };

ZSTD_HOT unsigned ll_code(proven_u32 ll) { return ll < 64 ? LL_CODE[ll] : highbit32(ll) + 19; }
ZSTD_HOT unsigned ml_code(proven_u32 ml) { const proven_u32 b = ml - 3; return b < 128 ? ML_CODE[b] : highbit32(b) + 36; }

static proven_size_t lits_header_raw(proven_byte_t *d, unsigned type, proven_size_t n) {
    if (n < 32) { d[0] = (proven_byte_t)(type | (n << 3)); return 1; }
    if (n < 4096) { const proven_u32 v = type | (1u << 2) | ((proven_u32)n << 4); d[0] = (proven_byte_t)v; d[1] = (proven_byte_t)(v >> 8); return 2; }
    const proven_u32 v = type | (3u << 2) | ((proven_u32)n << 4);
    d[0] = (proven_byte_t)v; d[1] = (proven_byte_t)(v >> 8); d[2] = (proven_byte_t)(v >> 16);
    return 3;
}

static proven_size_t lits_raw(proven_byte_t *d, proven_size_t cap, const proven_byte_t *lit, proven_size_t n) {
    if (cap < n + 3) return 0;
    const proven_size_t h = lits_header_raw(d, 0, n);
    if (n) (void)proven_mem_copy(d + h, n, (proven_mem_view_t){ .ptr = lit, .size = n });
    return h + n;
}

/* The literals section (section 3.1.1.3.1). `e` is the entropy in force; the code it makes,
 * if any, goes into the other slot and `*huf_next` says which slot the next block will have. */
ZSTD_HOT proven_size_t write_literals(proven_zstd_encoder_t *z, proven_byte_t *d, proven_size_t cap, int *huf_next) {
    const proven_byte_t *lit = z->lits;
    const proven_size_t n = z->nlit;
    proven_entropy_t *e = &z->ent;
    *huf_next = e->huf_cur;
    const bool can_repeat = e->huf_cur >= 0;
    if (n <= (can_repeat ? 6u : 63u)) return lits_raw(d, cap, lit, n);
    /* Four counts side by side: a run of one byte value does not wait on itself. */
    proven_u32 count[256], c4[4][256] = { { 0 } };
    proven_size_t i = 0;
    for (; i + 4 <= n; i += 4) { c4[0][lit[i]]++; c4[1][lit[i + 1]]++; c4[2][lit[i + 2]]++; c4[3][lit[i + 3]]++; }
    for (; i < n; ++i) c4[0][lit[i]]++;
    for (unsigned v = 0; v < 256; ++v) count[v] = c4[0][v] + c4[1][v] + c4[2][v] + c4[3][v];
    unsigned maxsym = 255;
    while (count[maxsym] == 0) maxsym--;
    if (count[maxsym] == n) {
        if (cap < 4) return 0;
        const proven_size_t h = lits_header_raw(d, 1, n);
        d[h] = lit[0];
        return h + 1;
    }
    /* A new code, or the one in force if it serves these symbols for fewer bytes. */
    const int slot = e->huf_cur == 0 ? 1 : 0;
    proven_huf_tab_t *nt = &e->huf[slot];
    huf_build(nt, count, maxsym);
    proven_u64 bits_new = 0, bits_old = ~(proven_u64)0;
    for (unsigned s = 0; s <= maxsym; ++s) bits_new += (proven_u64)count[s] * nt->bits[s];
    proven_byte_t desc[160];
    const proven_size_t dlen = huf_describe(desc, sizeof desc, nt);
    if (can_repeat) {
        const proven_huf_tab_t *ot = &e->huf[e->huf_cur];
        bits_old = 0;
        for (unsigned s = 0; s <= maxsym && bits_old != ~(proven_u64)0; ++s) {
            if (count[s] == 0) continue;
            if (s > ot->maxsym || ot->bits[s] == 0) bits_old = ~(proven_u64)0;
            else bits_old += (proven_u64)count[s] * ot->bits[s];
        }
    }
    const bool repeat = bits_old != ~(proven_u64)0 && (dlen == 0 || bits_old / 8 <= bits_new / 8 + dlen);
    if (!repeat && dlen == 0) return lits_raw(d, cap, lit, n);
    const proven_huf_tab_t *t = repeat ? &e->huf[e->huf_cur] : nt;
    const bool four = n >= 256;
    const proven_size_t hs = !four ? 3 : n < 1024 ? 3 : n < 16384 ? 4 : 5;
    if (cap < hs + 1 + SLACK) return 0;
    proven_byte_t *o = d + hs, *const oend = d + cap;
    if (!repeat) {
        if ((proven_size_t)(oend - o) < dlen) return 0;
        (void)proven_mem_copy(o, dlen, (proven_mem_view_t){ .ptr = desc, .size = dlen });
        o += dlen;
    }
    if (!four) {
        const proven_size_t k = huf_stream(o, (proven_size_t)(oend - o), lit, n, t);
        if (k == 0) return lits_raw(d, cap, lit, n);
        o += k;
    } else {
        if (oend - o < 6) return lits_raw(d, cap, lit, n);
        proven_byte_t *jump = o;
        o += 6;
        const proven_size_t seg = (n + 3) / 4;
        for (int s = 0; s < 4; ++s) {
            const proven_size_t len = s < 3 ? seg : n - 3 * seg;
            const proven_size_t k = huf_stream(o, (proven_size_t)(oend - o), lit + (proven_size_t)s * seg, len, t);
            if (k == 0 || (s < 3 && k > 0xFFFF)) return lits_raw(d, cap, lit, n);
            if (s < 3) { jump[2 * s] = (proven_byte_t)k; jump[2 * s + 1] = (proven_byte_t)(k >> 8); }
            o += k;
        }
    }
    const proven_size_t csize = (proven_size_t)(o - d) - hs;
    const proven_size_t min_gain = (n >> 6) + 2;
    if (csize + hs >= n - (n < min_gain ? n : min_gain)) return lits_raw(d, cap, lit, n);
    const proven_u64 type = repeat ? 3u : 2u;
    if (hs == 3) {
        const proven_u32 v = (proven_u32)type | ((four ? 1u : 0u) << 2) | ((proven_u32)n << 4) | ((proven_u32)csize << 14);
        d[0] = (proven_byte_t)v; d[1] = (proven_byte_t)(v >> 8); d[2] = (proven_byte_t)(v >> 16);
    } else if (hs == 4) {
        const proven_u32 v = (proven_u32)type | (2u << 2) | ((proven_u32)n << 4) | ((proven_u32)csize << 18);
        d[0] = (proven_byte_t)v; d[1] = (proven_byte_t)(v >> 8); d[2] = (proven_byte_t)(v >> 16); d[3] = (proven_byte_t)(v >> 24);
    } else {
        const proven_u64 v = type | (3u << 2) | ((proven_u64)n << 4) | ((proven_u64)csize << 22);
        for (int i = 0; i < 5; ++i) d[i] = (proven_byte_t)(v >> (8 * i));
    }
    if (!repeat) *huf_next = slot;
    return (proven_size_t)(o - d);
}

/* Choose the table for one kind of code: predefined (mode 0), a single symbol (1), one
 * written here (2) or the one in force (3), by what each would cost. Its description, if
 * any, goes to `d`. */
static int choose_table(proven_zstd_encoder_t *z, int kind, const proven_u32 *count, unsigned maxsym, proven_size_t nseq, unsigned maxlog,
                        proven_byte_t *d, proven_size_t cap, proven_size_t *dlen, const proven_fse_tab_t **use, int *next) {
    proven_entropy_t *e = &z->ent;
    const int cur = e->cur[kind];
    const int slot = cur == 0 ? 1 : 0;
    *dlen = 0;
    *next = cur;
    proven_u32 most = 0;
    for (unsigned s = 0; s <= maxsym; ++s) if (count[s] > most) most = count[s];
    static const proven_i16 *const DEF_NORM[3] = { LL_DEFAULT, OF_DEFAULT, ML_DEFAULT };
    static const unsigned DEF_NSYM[3] = { 36, 29, 53 }, DEF_LOG[3] = { 6, 5, 6 };
    proven_fse_tab_t *const def = &z->def[kind];
    const proven_u64 c_def = fse_cost(count, maxsym, DEF_NORM[kind], DEF_NSYM[kind], DEF_LOG[kind]);
    if (!z->def_built[kind]) {
        fse_tab_build(def, DEF_NORM[kind], DEF_NSYM[kind], DEF_LOG[kind]);
        z->def_built[kind] = true;
    }
    if (most == nseq) {
        if (nseq <= 2 && c_def != ~(proven_u64)0) { *use = def; *next = 2; return 0; }
        if (cap < 1) return -1;
        fse_tab_rle(&e->tab[kind][slot], maxsym);
        d[0] = (proven_byte_t)maxsym;
        *dlen = 1;
        *use = &e->tab[kind][slot];
        *next = slot;
        return 1;
    }
    proven_u64 c_old = ~(proven_u64)0;
    const proven_fse_tab_t *old = cur == 2 ? def : cur >= 0 ? &e->tab[kind][cur] : NULL;
    if (old && cur != 2) c_old = fse_cost(count, maxsym, old->norm, old->nsym, old->log);
    proven_i16 norm[64];
    const unsigned log = fse_log(nseq, maxsym, maxlog);
    const proven_size_t k = fse_normalize(norm, log, count, nseq, maxsym) ? fse_describe(d, cap, norm, maxsym, log) : 0;
    proven_u64 c_new = ~(proven_u64)0;
    if (k > 0) {
        c_new = fse_cost(count, maxsym, norm, maxsym + 1, log);
        c_new += (proven_u64)k * 8 * 256;
    }
    if (c_def <= c_new && c_def <= c_old) { *use = def; *next = 2; return 0; }
    if (c_old <= c_new) { *use = old; return 3; }
    if (k == 0) return -1;
    fse_tab_build(&e->tab[kind][slot], norm, maxsym + 1, log);
    *dlen = k;
    *use = &e->tab[kind][slot];
    *next = slot;
    return 2;
}

/* The sequences section (section 3.1.1.3.2), into `d`. */
ZSTD_HOT proven_size_t write_sequences(proven_zstd_encoder_t *z, proven_byte_t *d, proven_size_t cap, int next[3]) {
    const proven_size_t nseq = z->nseq;
    next[0] = z->ent.cur[0]; next[1] = z->ent.cur[1]; next[2] = z->ent.cur[2];
    if (cap < 4) return 0;
    proven_size_t o;
    if (nseq < 128) { d[0] = (proven_byte_t)nseq; o = 1; }
    else if (nseq < 0x7F00) { d[0] = (proven_byte_t)((nseq >> 8) + 0x80); d[1] = (proven_byte_t)nseq; o = 2; }
    else { d[0] = 0xFF; d[1] = (proven_byte_t)(nseq - 0x7F00); d[2] = (proven_byte_t)((nseq - 0x7F00) >> 8); o = 3; }
    if (nseq == 0) return o;
    const proven_seq_t *sq = z->seqs;
    proven_u32 cll[64] = { 0 }, cof[64] = { 0 }, cml[64] = { 0 };
    unsigned mll = 0, mof = 0, mml = 0;
    proven_byte_t *const code = z->codes;
    for (proven_size_t i = 0; i < nseq; ++i) {
        const unsigned a = ll_code(sq[i].ll), b = highbit32(sq[i].ofv), c = ml_code(sq[i].ml);
        code[3 * i] = (proven_byte_t)a; code[3 * i + 1] = (proven_byte_t)b; code[3 * i + 2] = (proven_byte_t)c;
        cll[a]++; cof[b]++; cml[c]++;
        if (a > mll) mll = a;
        if (b > mof) mof = b;
        if (c > mml) mml = c;
    }
    proven_byte_t *modes = d + o++;
    const proven_fse_tab_t *tl, *tf, *tm;
    proven_size_t k;
    const int ml_ = choose_table(z, K_LL, cll, mll, nseq, LL_LOG_MAX, d + o, cap - o, &k, &tl, &next[K_LL]);
    if (ml_ < 0) return 0;
    o += k;
    const int mo_ = choose_table(z, K_OF, cof, mof, nseq, OF_LOG_MAX, d + o, cap - o, &k, &tf, &next[K_OF]);
    if (mo_ < 0) return 0;
    o += k;
    const int mm_ = choose_table(z, K_ML, cml, mml, nseq, ML_LOG_MAX, d + o, cap - o, &k, &tm, &next[K_ML]);
    if (mm_ < 0) return 0;
    o += k;
    *modes = (proven_byte_t)((ml_ << 6) | (mo_ << 4) | (mm_ << 2));
    proven_bw_t b;
    if (!bw_init(&b, d + o, cap - o)) return 0;
    /* The last sequence first; its codes give the first states. */
    proven_size_t i = nseq - 1;
    unsigned lc = code[3 * i], oc = code[3 * i + 1], mc = code[3 * i + 2];
    proven_u32 sl = fse_first(tl, lc), so = fse_first(tf, oc), sm = fse_first(tm, mc);
    bw_add(&b, sq[i].ll - LL_BASE[lc], LL_BITS[lc]);
    bw_add(&b, sq[i].ml - ML_BASE[mc], ML_BITS[mc]);
    bw_flush(&b);
    bw_add(&b, sq[i].ofv - (1u << oc), oc);
    bw_flush(&b);
    while (i-- > 0) {
        lc = code[3 * i]; oc = code[3 * i + 1]; mc = code[3 * i + 2];
        fse_put(&b, &so, tf, oc);
        fse_put(&b, &sm, tm, mc);
        fse_put(&b, &sl, tl, lc);
        bw_add(&b, sq[i].ll - LL_BASE[lc], LL_BITS[lc]);
        if (LL_BITS[lc] + ML_BITS[mc] + oc > 30) bw_flush(&b);
        bw_add(&b, sq[i].ml - ML_BASE[mc], ML_BITS[mc]);
        bw_add(&b, sq[i].ofv - (1u << oc), oc);
        bw_flush(&b);
    }
    bw_add_low(&b, sm, tm->log);
    bw_add_low(&b, so, tf->log);
    bw_add_low(&b, sl, tl->log);
    const proven_size_t s = bw_close(&b);
    return s ? o + s : 0;
}

static bool all_same(const proven_byte_t *p, proven_size_t n) {
    for (proven_size_t i = 1; i < n; ++i) if (p[i] != p[0]) return false;
    return true;
}

/* One block of `src` (`n` bytes) whose sequences and literals are stored, with its header,
 * into `d`. Returns the bytes written, or 0 when `cap` is too small. A block that does not
 * shrink goes out stored, a run of one byte as such; the decoder's state from the last block
 * is then left as it was. */
ZSTD_HOT proven_size_t write_block(proven_zstd_encoder_t *z, const proven_byte_t *src, proven_size_t n, bool last, const proven_u32 rep_before[3],
                                 proven_byte_t *d, proven_size_t cap) {
    if (cap < 3) return 0;
    int huf_next = z->ent.huf_cur, next[3] = { z->ent.cur[0], z->ent.cur[1], z->ent.cur[2] };
    proven_size_t size = 0;
    const proven_size_t room = cap - 3 < n ? cap - 3 : n;           /* a compressed block must be smaller than the input */
    if (n > 0 && room > 4) {
        const proven_size_t a = write_literals(z, d + 3, room, &huf_next);
        if (a > 0) {
            const proven_size_t b = write_sequences(z, d + 3 + a, room - a, next);
            if (b > 0) size = a + b;
        }
    }
    const proven_size_t min_gain = (n >> 6) + 2;
    unsigned type = 2;
    if (size == 0 || size + min_gain > n) {
        z->rep[0] = rep_before[0]; z->rep[1] = rep_before[1]; z->rep[2] = rep_before[2];
        if (n > 1 && all_same(src, n)) {
            if (cap < 4) return 0;
            type = 1;
            d[3] = src[0];
            size = 1;
        } else {
            if (cap < 3 + n) return 0;
            type = 0;
            if (n) (void)proven_mem_copy(d + 3, n, (proven_mem_view_t){ .ptr = src, .size = n });
            size = n;
        }
    } else {
        z->ent.huf_cur = huf_next;
        z->ent.cur[0] = next[0]; z->ent.cur[1] = next[1]; z->ent.cur[2] = next[2];
    }
    const proven_u32 h = (last ? 1u : 0u) | (type << 1) | (proven_u32)((type == 1 ? n : size) << 3);
    d[0] = (proven_byte_t)h; d[1] = (proven_byte_t)(h >> 8); d[2] = (proven_byte_t)(h >> 16);
    return 3 + size;
}

// -----------------------------------------------------------------------------
// Long-distance matching
// -----------------------------------------------------------------------------

static const proven_u64 GEAR[256] = {
    0x69e93ff3099f06a9ull, 0x7135deef681aae6bull, 0x024fcc549baff8f0ull, 0x7e197af8c1c31b79ull,
    0x67fd8c9155a4e5a7ull, 0x2a97c97200da2bf8ull, 0xc4acf195d5416dbaull, 0x7b59f778d68f10bdull,
    0x2e65d2fdab7f42eeull, 0x71f122c93a01cea5ull, 0x3581224d446db9b9ull, 0x0f067813bb1676a3ull,
    0x8fabd4fe0e084741ull, 0x034f40a0951e52dcull, 0xda7590b6f4ac392full, 0xccc082d669f2d597ull,
    0x1e9fb9d18859dcafull, 0x33887e852b040612ull, 0xa218fdf69c1f0fd3ull, 0x9208fc395f239381ull,
    0x5d0b992bac64e3b8ull, 0x8b71a1722235c897ull, 0xb0f27c036a8f514eull, 0x5f357802b3328dffull,
    0x090478ae2544520eull, 0x0a630c620116e36eull, 0x1382374393b86a4dull, 0xd790913b6befed1eull,
    0xe7a52cdaaa35e5d3ull, 0x04c5197c49526ec2ull, 0x2bf35d30dfb575a2ull, 0x41ca1ab954b43192ull,
    0xfac7f3de011e6cfbull, 0xc15439f07d88d515ull, 0x6fac23a5909c2506ull, 0x72ba894ad7124f83ull,
    0x175560263aeab97eull, 0x9059553d1d28564bull, 0xc87a36fa9121e333ull, 0x2e581549d86c7468ull,
    0x5f2ff2d6844160c5ull, 0x7ee3fb8c5f965a94ull, 0xcfe1bf713e4fcbccull, 0xb6800fb2aef03584ull,
    0x394ac0d40db4aaecull, 0x5cdeb464117d3e1full, 0x82842ebb737fad96ull, 0xea07725e5acbd2a8ull,
    0x362c120a18921390ull, 0xc8fc04cfaa6c790dull, 0xd9dc7b9f6e045a99ull, 0x806d127f8385943full,
    0x084e5e44717d8818ull, 0x9834f06459cecbc9ull, 0x72e51357f9bb6dedull, 0x8ab34a8d4aa3d051ull,
    0x923af89babee9e53ull, 0x0acf7827462459c0ull, 0x8fb1e8f136457400ull, 0x7d5d101a9a234606ull,
    0x4f0b2cdfaca19e8full, 0x4e6e4cd6a1be53c1ull, 0x7f8c960c12895171ull, 0xc4044b5b6b57c1dcull,
    0x07b8a97a422ac271ull, 0x9b96d3b5ecfe416bull, 0x68e85a40328b4b99ull, 0xbc9cda8ab0285872ull,
    0x654bab60b014fb46ull, 0x416d3f5eba9b051dull, 0xd1e07c020873c306ull, 0xf0d130423c2ebfb9ull,
    0xf02cc7e75b4799a7ull, 0xb82bfd134a931c30ull, 0x9125b574ed4a9cacull, 0x2c119225d0878770ull,
    0xbcb0a8ae07568c92ull, 0x844b135a7e6de06bull, 0x4db84013539e6ca1ull, 0x98f0c138ae7fd461ull,
    0x151dc8d8801ab15bull, 0xc358b52a9cdb2ee3ull, 0xdee4218c16d5f8c1ull, 0x514bf84d07b973f9ull,
    0x2d48724dbc28f66eull, 0x1ef5d1ac4ea7010dull, 0x4a9440a4a8c3f878ull, 0x78724d15d2a317abull,
    0x0b72638e658e3c0cull, 0xafa697800294296cull, 0x6a2fccec34bbc21bull, 0x320a000f4572a667ull,
    0x7c339e479dde83c1ull, 0x3414e3b599271ff5ull, 0xe686ba7fa1c8cddcull, 0xde4a12f0716a0d82ull,
    0x02f02544190f0562ull, 0x3677360007d80e13ull, 0x4c9f6da8378ad0f8ull, 0x9fbe3e79777ff75dull,
    0xc839f93576d76e84ull, 0x8bed4e2387122d22ull, 0x4909baba143c8395ull, 0x13fb5a787bf9dc61ull,
    0x4dba24b81d6597aeull, 0x2843ea32d7ef93d2ull, 0x386655a91e4a152bull, 0x3ae67326aa0ae56aull,
    0x8f3f9955fb1e4043ull, 0x9844273c3b7594bcull, 0x038f040f89dc5a95ull, 0xb931c9f4cff320ddull,
    0x4ca4458cfeb1fd9eull, 0xa603d61c6d351412ull, 0xd63d6a167acf52d2ull, 0x642de13468e8917cull,
    0x2e9f041ac836348cull, 0x7c958e15136551c6ull, 0x0fe0deb2fe037aaeull, 0x02f34512a9cfddbbull,
    0x7691b3c67be4dffbull, 0x0a282e2c0a526718ull, 0xbb6124b8f6d4dda8ull, 0x27b0e123773c6aacull,
    0x51615c1b8fb17357ull, 0xfc4920038ceacb4full, 0x24269ef5f9dc7018ull, 0x28cc0d5969fd52faull,
    0xb736ad44fff002e9ull, 0x27f898269e0eacb5ull, 0xfe5fd561db788e9aull, 0x105f6e207e6607b2ull,
    0x67f191ce0e5d0322ull, 0x6896ed95e6ba4762ull, 0x5c3734888fd7b78dull, 0x6f2e64304dd52df2ull,
    0x9083e17a08530372ull, 0x7164591467313099ull, 0x6c44d24fb4ae0225ull, 0x6cd641e1fa5abdaaull,
    0xb651218018a96d03ull, 0x933e00ed2631e23aull, 0x72f7e5db0f90a4a7ull, 0x31f0452cc111969bull,
    0x130874c50cb08385ull, 0x777e3b2d3dacde91ull, 0x68334110a5a3b54eull, 0x55c54c8a51541478ull,
    0xa2204f185f28432aull, 0x2dab374dc42c8a4full, 0x56042e3147e504a5ull, 0x963333434c7be766ull,
    0x1776aa709efc2d27ull, 0x3dd3c43fdd36e09full, 0xbddd46dde7bd2243ull, 0x3d817245355a58d1ull,
    0x11ee3ab30c4b04bdull, 0x7446b4c27759cc24ull, 0x192f8aa6c5612b3aull, 0x562cfc7d7322366bull,
    0x83b1ba5f1d50feedull, 0xa88ee947be3c8580ull, 0x5664e7b52088827full, 0xe90532fa19a1e3f5ull,
    0x7f5ec021d189ab85ull, 0x2aacc7eb5f3fb601ull, 0xd19f840e71fe5f71ull, 0x5785bbd25ad33a03ull,
    0x8e18d45e4da03f16ull, 0xe89488af3f32d3a9ull, 0x95ec6791532be198ull, 0x069cb88a6eac9d98ull,
    0x4772dacbecbb700dull, 0x80440f3af1cb4456ull, 0x034033033b61b938ull, 0x8199ed963c838953ull,
    0xe117055ad1791be8ull, 0x86e6f818ababc821ull, 0x762e228cf90cec69ull, 0x1f8434ce969f7984ull,
    0x3512f299ad2e3bb8ull, 0xa9ba16a0d2e3ef17ull, 0xc04d3db4effa4caaull, 0x2b9941f9d601efa4ull,
    0xe5a6257a1eaf7435ull, 0x479185e5c183352full, 0xb8f061d85af3d02full, 0x2af760fce7878725ull,
    0xecfa80bddacdd8f9ull, 0xf1817d32600f3affull, 0xf25f2125b4898adfull, 0xf7c30d03981c7b2cull,
    0x728184818d9731d3ull, 0x5d929ab59a7c1f13ull, 0x1284c34c463b801dull, 0x67417938e8b41b7dull,
    0xd184e048a459799eull, 0xd7145a3cf6cde387ull, 0x0140a5f3a9847451ull, 0x279dc811cce2d204ull,
    0x711d6261a78cf3cbull, 0xc9978d4087fd5173ull, 0x70e68586d3976951ull, 0x0f5d25af274681bdull,
    0x92455831377cf3a0ull, 0xbd2f7f86a0f63293ull, 0x2abc2ba6afcc1dabull, 0x31654d81514992f0ull,
    0x289b1238990c4353ull, 0xed6a0058ef388531ull, 0x5a2f8ab39245fe0aull, 0xbe99be6b5f8511f4ull,
    0x1c812880cfb40bc4ull, 0xb1bb1bcecd4b6e23ull, 0xa1fe5cda5a229acaull, 0x1a6ab96436807575ull,
    0x55a16a678bf42e7full, 0x18f2cc3054e1a4e0ull, 0xce5364add5a46058ull, 0x1d21034f6bee3800ull,
    0xd20143a4be8c6c20ull, 0x33e3f386dd609e20ull, 0xb9efd1b51abe325full, 0x8bdd0839dbbb3f0full,
    0x879ac2c020391313ull, 0x27109fb160f77b88ull, 0x172bd70543541628ull, 0xd52c8098694d6dd9ull,
    0x7ec6b8bf76b31ac0ull, 0x8bc4bc034f7f622bull, 0x57d23cfb17a8d973ull, 0x6490b4321ff618cfull,
    0x1cc06ff2f4364f5full, 0xeb552732a8fe6917ull, 0x5dcca698f32acfcaull, 0x5598fcb0e9298719ull,
    0x3381138fbfed67c6ull, 0x3a8854aaefece6fdull, 0x57d635b45219e809ull, 0xf733a7aa57a4b991ull,
    0x6f3c5fa9b497d80eull, 0x82100103afcab03aull, 0xbf3cbf5f01d0117full, 0x80c487f63ad6f232ull,
    0x47320c7a33d2d768ull, 0xd8775ca17d0007e2ull, 0xb01d99b6c8b2dc99ull, 0xdbfb9d4aeb37c056ull,
    0xc46402cce3f802bbull, 0x8e090d6a8cc5f6fcull, 0x6a36c6be8478f9aaull, 0x07b44db8319af415ull,
    0xcf6545b853f21725ull, 0x11bc1b1b8d15fd13ull, 0xca28fa0eca6110e4ull, 0x5d19a2deb9129aaeull,
};

/* The long matches of `base` + [start, end), in order and apart, each LDM_MIN bytes or more, from no
 * further back than `low`; every position the hash picks is put in its bucket. Returns how many. */
static proven_size_t ldm_find(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end, proven_size_t low) {
    if (end - start < LDM_MIN) return 0;
    const proven_u64 stop = (((proven_u64)1 << LDM_RATE_LOG) - 1) << (64 - LDM_RATE_LOG);
    const unsigned bucket_bits = z->ldm_log - LDM_BUCKET_LOG;
    const proven_byte_t *const iend = base + end;
    proven_size_t n = 0, anchor = start, p = start;
    proven_u64 h = 0;
    for (proven_size_t q = start; q < start + LDM_MIN - 1; ++q) h = (h << 1) + GEAR[base[q]];
    p = start + LDM_MIN - 1;
    while (p < end) {
        h = (h << 1) + GEAR[base[p]];
        p++;
        if (h & stop) continue;
        /* The 64 bytes before p: a position to remember, and to look up. */
        const proven_size_t c = p - LDM_MIN;
        const proven_u64 mix = h * 0x9E3779B97F4A7C15ull;
        const proven_u32 b = (proven_u32)(mix >> (64 - bucket_bits)), check = (proven_u32)(mix >> 8);
        proven_u64 *const bk = z->ldm + ((proven_size_t)b << LDM_BUCKET_LOG);
        proven_size_t best = 0, best_back = 0, best_from = 0;
        if (c >= anchor) {
            for (unsigned i = 0; i < (1u << LDM_BUCKET_LOG); ++i) {
                const proven_u32 at = (proven_u32)bk[i];
                if ((proven_u32)(bk[i] >> 32) != check || at < low || at >= c) continue;
                const proven_size_t fwd = count_same(base + c, base + at, iend);
                if (fwd < LDM_MIN) continue;
                proven_size_t back = 0;
                while (c - back > anchor && at - back > low && base[c - back - 1] == base[at - back - 1]) back++;
                if (fwd + back > best) { best = fwd + back; best_back = back; best_from = at; }
            }
        }
        bk[z->ldm_next[b]] = ((proven_u64)check << 32) | (proven_u32)c;
        z->ldm_next[b] = (proven_u8)((z->ldm_next[b] + 1) & ((1u << LDM_BUCKET_LOG) - 1));
        if (best == 0) continue;
        if (n == z->ldm_cap) break;
        z->ldm_m[n++] = (struct proven_ldm_match){ (proven_u32)(c - best_back), (proven_u32)(c - best_from), (proven_u32)best };
        anchor = c - best_back + best;
        if (anchor + LDM_MIN > end) break;
        /* Hash again from the match's end: a run inside it would pick the same bytes over and over. */
        h = 0;
        for (proven_size_t q = anchor; q < anchor + LDM_MIN - 1; ++q) h = (h << 1) + GEAR[base[q]];
        p = anchor + LDM_MIN - 1;
    }
    return n;
}

/* The block's long matches as sequences, and the level's matcher between them. Returns where the
 * block ends, as match_block. */
static proven_size_t match_ldm(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end, proven_size_t low, proven_size_t *last_from) {
    const proven_size_t n = ldm_find(z, base, start, end, low);
    proven_size_t cur = start;
    *last_from = start;
    for (proven_size_t i = 0; i < n; ++i) {
        const struct proven_ldm_match m = z->ldm_m[i];
        proven_size_t lf = cur;
        if (m.at - cur >= 16) {
            const proven_size_t stop = match_block(z, base, cur, m.at, low, &lf);
            if (stop < m.at) { *last_from = lf; return stop; }
        }
        if (z->seq_max - z->nseq < 3) { *last_from = lf; return m.at; }
        proven_sink_t k = sink_open(z);
        store(&k, base + lf, base + end, m.at - lf, m.off, m.len);
        sink_close(z, &k);
        cur = m.at + m.len;
        *last_from = cur;
    }
    if (end - cur >= 16) return match_block(z, base, cur, end, low, last_from);
    return end;
}

/* Match and write one block: `base` + [start, end). Returns the bytes written into `d` (0:
 * `cap` too small) and where the block ended. */
ZSTD_HOT proven_size_t compress_block(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end, bool last,
                                    proven_byte_t *d, proven_size_t cap, proven_size_t *ended) {
    const proven_u32 rep_before[3] = { z->rep[0], z->rep[1], z->rep[2] };
    z->nseq = 0;
    z->nlit = 0;
    /* While the frame has produced no more than its window, a match may reach any byte of the
     * dictionary's content before it (RFC 8878, section 5). */
    const proven_size_t low = z->dict_at && end - z->dict_at <= z->window ? 0 : end > z->window ? end - z->window : 0;
    proven_size_t last_from = start, stop = end;
    if (z->ldm) stop = match_ldm(z, base, start, end, low, &last_from);
    else if (end - start >= 16) stop = match_block(z, base, start, end, low, &last_from);
    else last_from = start;
    const proven_size_t tail = stop - last_from;
    if (tail) (void)proven_mem_copy(z->lits + z->nlit, tail, (proven_mem_view_t){ .ptr = base + last_from, .size = tail });
    z->nlit += tail;
    *ended = stop;
    return write_block(z, base + start, stop - start, last && stop == end, rep_before, d, cap);
}

/* The block coder twice: as the build compiles it, and on x86 once more with BMI2 allowed - the
 * hash and the bit writer shift by amounts in registers, which BMI2 does without the flags
 * (about 6 % faster, RFC-0028 S-5). The same C and the same output; which one runs is asked of
 * the processor once. */
static proven_size_t block_plain(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end, bool last,
                                 proven_byte_t *d, proven_size_t cap, proven_size_t *ended) {
    return compress_block(z, base, start, end, last, d, cap, ended);
}

#if ZSTD_BMI2
__attribute__((target("bmi2")))
static proven_size_t block_bmi2(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end, bool last,
                                proven_byte_t *d, proven_size_t cap, proven_size_t *ended) {
    return compress_block(z, base, start, end, last, d, cap, ended);
}

static int g_zstd_bmi2 = -1;                     /* -1: not asked yet */

void proven_zstd_encode_force_plain_(bool on) {
    __atomic_store_n(&g_zstd_bmi2, on ? 0 : -1, __ATOMIC_RELAXED);
}

static bool use_bmi2(void) {
    int v = __atomic_load_n(&g_zstd_bmi2, __ATOMIC_RELAXED);
    if (v < 0) {
        v = proven_sys_cpu_bmi2() ? 1 : 0;
        __atomic_store_n(&g_zstd_bmi2, v, __ATOMIC_RELAXED);
    }
    return v != 0;
}
#else
void proven_zstd_encode_force_plain_(bool on) { (void)on; }
#endif

static proven_size_t encode_block(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t start, proven_size_t end, bool last,
                                  proven_byte_t *d, proven_size_t cap, proven_size_t *ended) {
#if ZSTD_BMI2
    if (use_bmi2()) return block_bmi2(z, base, start, end, last, d, cap, ended);
#endif
    return block_plain(z, base, start, end, last, d, cap, ended);
}

// -----------------------------------------------------------------------------
// Frames
// -----------------------------------------------------------------------------

static proven_size_t frame_header(proven_byte_t *d, unsigned wlog, bool check, bool sized, proven_u64 size, proven_u32 did) {
    d[0] = 0x28; d[1] = 0xB5; d[2] = 0x2F; d[3] = 0xFD;
    const bool single = sized && size <= ((proven_u64)1 << wlog);
    const unsigned fcs = !sized ? 0u : (single && size < 256) ? 0u : size < 65536 + 256 ? 1u : size <= 0xFFFFFFFFu ? 2u : 3u;
    const unsigned dflag = did == 0 ? 0u : did < 256 ? 1u : did < 65536 ? 2u : 3u;
    d[4] = (proven_byte_t)((fcs << 6) | (single ? 0x20u : 0u) | (check ? 4u : 0u) | dflag);
    proven_size_t o = 5;
    if (!single) d[o++] = (proven_byte_t)((wlog - 10) << 3);
    for (unsigned i = 0; i < (dflag == 3 ? 4u : dflag); ++i) d[o++] = (proven_byte_t)(did >> (8 * i));
    if (sized) {
        const proven_u64 v = fcs == 1 ? size - 256 : size;
        const unsigned bytes = fcs == 0 ? 1u : fcs == 1 ? 2u : fcs == 2 ? 4u : 8u;
        for (unsigned i = 0; i < bytes; ++i) d[o++] = (proven_byte_t)(v >> (8 * i));
    }
    return o;
}

/* Room for one block's output, its header and the slack of eight-byte stores. */
static proven_size_t block_out_max(const proven_zstd_encoder_t *z) { return z->block_size + 3 + SLACK; }

static void put_check(proven_zstd_encoder_t *z, proven_byte_t *d) {
    const proven_u32 c = (proven_u32)xxh_digest(&z->xxh);
    d[0] = (proven_byte_t)c; d[1] = (proven_byte_t)(c >> 8); d[2] = (proven_byte_t)(c >> 16); d[3] = (proven_byte_t)(c >> 24);
}

/* Slide the buffer: keep the window before `pos`, and move the hash tables with it. */
static void slide(proven_zstd_encoder_t *z) {
    if (z->pos <= z->window) return;
    const proven_size_t shift = z->pos - z->window;
    (void)proven_mem_move(z->buf, z->fill - shift, (proven_mem_view_t){ .ptr = z->buf + shift, .size = z->fill - shift });
    z->pos -= shift;
    z->fill -= shift;
    z->dict_at = 0;
    const proven_u32 s = (proven_u32)shift;
    if (z->p.strategy >= 2) {
        /* The chain is indexed by position modulo its size, the trees by twice that: what was at p
         * is now at p - s. Rotate left by s (two links a position for the trees), in place (three
         * reversals). */
        proven_u32 *c = z->hash2;
        const proven_size_t n = z->hash2_size, r = (z->p.strategy >= 3 ? 2 * shift : shift) & (n - 1);
        for (int part = 0; part < 3; ++part) {
            proven_size_t a = part == 1 ? r : 0, b = part == 0 ? r : n;
            while (a + 1 < b) { const proven_u32 t = c[a]; c[a] = c[b - 1]; c[b - 1] = t; a++; b--; }
        }
        z->next_update = z->next_update > s ? z->next_update - s : 0;
    }
    for (proven_size_t i = 0; i < z->hash_size + z->hash2_size; ++i) z->hash[i] = z->hash[i] > s ? z->hash[i] - s : 0;
    if (z->ldm) {
        for (proven_size_t i = 0; i < ((proven_size_t)1 << z->ldm_log); ++i) {
            const proven_u32 at = (proven_u32)z->ldm[i];
            z->ldm[i] = at > s ? (z->ldm[i] & 0xFFFFFFFF00000000ull) | (at - s) : 0;
        }
    }
    if (z->optp) {
        for (proven_size_t i = 0; i < ((proven_size_t)1 << z->optp->h3_log); ++i) z->optp->h3[i] = z->optp->h3[i] > s ? z->optp->h3[i] - s : 0;
        z->optp->h3_next = z->optp->h3_next > s ? z->optp->h3_next - s : 0;
    }
}

/* Compress from `pos` as far as `upto`, the last block marked if asked, into `pend`. */
/* The dictionary's content in `base` + [0, n): its positions put in the hash tables of levels 1 to 3
 * (the chains and trees of 4 and up take them as they go, from `next_update`). */
static void dict_load(proven_zstd_encoder_t *z, const proven_byte_t *base, proven_size_t n) {
    z->fresh = false;
    z->dict_at = n;
    if (n < 8 || z->p.strategy > 1) return;
    const unsigned hlog = z->p.hlog, clog = z->p.clog, mls = z->p.mls;
    for (proven_size_t p = 0; p + 8 <= n; ++p) {
        if (z->p.strategy == 0) z->hash[hash_at(base + p, hlog, 6)] = (proven_u32)p;
        else { z->hash[hash_at(base + p, hlog, 8)] = (proven_u32)p; z->hash2[hash_at(base + p, clog, mls)] = (proven_u32)p; }
    }
}

static proven_err_t emit(proven_zstd_encoder_t *z, proven_size_t upto, bool last) {
    proven_size_t need = z->pend_len + block_out_max(z) + 4;
    proven_err_t e = grow(z, (void **)&z->pend, &z->pend_cap, need, 1);
    if (e == PROVEN_OK) e = block_room(z);
    if (e != PROVEN_OK) return e;
    do {
        const proven_size_t to = upto - z->pos > z->block_size ? z->pos + z->block_size : upto;
        /* Position 0 of the buffer is the frame's first byte until the first slide, after
         * which `pos` is never below a window. */
        proven_size_t ended = to;
        const proven_byte_t *base = z->buf;
        proven_size_t k;
        k = encode_block(z, base, z->pos, to, last && to == upto, z->pend + z->pend_len, z->pend_cap - z->pend_len, &ended);
        if (k == 0) return PROVEN_ERR_INVALID_STATE;
        z->pend_len += k;
        z->pos = ended;
        if (z->pend_cap - z->pend_len < block_out_max(z) + 4 && z->pos < upto) {
            e = grow(z, (void **)&z->pend, &z->pend_cap, z->pend_len + block_out_max(z) + 4, 1);
            if (e != PROVEN_OK) return e;
        }
    } while (z->pos < upto);
    return PROVEN_OK;
}

proven_err_t proven_zstd_encode(proven_zstd_encoder_t *z, proven_mem_view_t in, proven_size_t *consumed,
                                proven_mem_mut_t out, proven_size_t *produced, proven_zstd_flush_t flush, bool *done) {
    if (consumed) *consumed = 0;
    if (produced) *produced = 0;
    if (done) *done = false;
    if (!z || !consumed || !produced || !done || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr) ||
        (unsigned)flush > (unsigned)PROVEN_ZSTD_FLUSH_FINISH) return PROVEN_ERR_INVALID_ARG;
    if (z->failed) return PROVEN_ERR_NOMEM;
    if (z->finished && in.size > 0) return PROVEN_ERR_INVALID_STATE;
    proven_size_t in_at = 0, out_at = 0;
    proven_err_t e = PROVEN_OK;
    for (;;) {
        /* Hand over what is waiting; nothing new is made while any of it is left. */
        if (z->pend_at < z->pend_len) {
            proven_size_t k = z->pend_len - z->pend_at;
            if (k > out.size - out_at) k = out.size - out_at;
            if (k) (void)proven_mem_copy(out.ptr + out_at, k, (proven_mem_view_t){ .ptr = z->pend + z->pend_at, .size = k });
            out_at += k;
            z->pend_at += k;
            if (z->pend_at < z->pend_len) break;
        }
        z->pend_len = z->pend_at = 0;
        if (z->finished) break;
        if (!z->header_done) {
            e = grow(z, (void **)&z->pend, &z->pend_cap, block_out_max(z) + 32, 1);
            if (e != PROVEN_OK) break;
            if (z->cd) {
                const proven_size_t n = z->cd->d.content_len;
                if (z->buf_cap < n + 1) {
                    e = grow(z, (void **)&z->buf, &z->buf_cap, n + 64 * 1024 < z->buf_max ? n + 64 * 1024 : z->buf_max, 1);
                    if (e != PROVEN_OK) break;
                }
                (void)proven_mem_copy(z->buf, n, (proven_mem_view_t){ .ptr = z->cd->d.content, .size = n });
                z->pos = z->fill = n;
                dict_load(z, z->buf, n);
            }
            z->pend_len = frame_header(z->pend, z->wlog, z->opt.checksum, z->stated != 0, z->stated, z->cd ? z->cd->d.id : 0);
            z->header_done = true;
            continue;
        }
        /* Take input. When the buffer is full and its whole blocks have gone, it slides - as far as
         * it can, for a slide goes through all the tables (80 MiB of them at level 19; sliding a
         * block at a time once made a stream twice as slow as in place). It grows until it is
         * full size. */
        if (in_at < in.size) {
            if (z->fill == z->buf_max && z->pos > z->window && z->fill - z->pos < z->block_size) slide(z);
            if (z->fill == z->buf_cap && z->buf_cap < z->buf_max) {
                proven_size_t want = z->buf_cap ? z->buf_cap * 2 : 64 * 1024;
                if (want < z->fill + (in.size - in_at)) want = z->fill + (in.size - in_at);
                if (want > z->buf_max) want = z->buf_max;
                e = grow(z, (void **)&z->buf, &z->buf_cap, want, 1);
                if (e != PROVEN_OK) break;
            }
            proven_size_t k = in.size - in_at;
            if (k > z->buf_cap - z->fill) k = z->buf_cap - z->fill;
            if (k) {
                (void)proven_mem_copy(z->buf + z->fill, k, (proven_mem_view_t){ .ptr = in.ptr + in_at, .size = k });
                if (z->opt.checksum) xxh_update(&z->xxh, in.ptr + in_at, k);
                z->fill += k;
                z->total += k;
                in_at += k;
            }
        }
        /* Whole blocks go as soon as more input follows them. */
        const proven_size_t ready = z->fill - z->pos;
        if (ready > z->block_size || (ready == z->block_size && in_at < in.size)) {
            e = emit(z, z->pos + z->block_size, false);
            if (e != PROVEN_OK) break;
            continue;
        }
        /* Input left: the buffer grows, or slides, at the top. Full and unable to slide, a
         * whole block would be ready and have gone above, so this does not loop. */
        if (in_at < in.size) {
            if (z->fill < z->buf_max || z->pos > z->window) continue;
            break;
        }
        if (flush == PROVEN_ZSTD_FLUSH_NONE) break;
        if (flush == PROVEN_ZSTD_FLUSH_FINISH) {
            if (z->fill > z->pos) {
                e = emit(z, z->fill, true);
                if (e != PROVEN_OK) break;
            } else {
                e = grow(z, (void **)&z->pend, &z->pend_cap, 8, 1);
                if (e != PROVEN_OK) break;
                z->pend[z->pend_len++] = 1;                 /* an empty stored block, the last */
                z->pend[z->pend_len++] = 0;
                z->pend[z->pend_len++] = 0;
            }
            if (z->opt.checksum) { put_check(z, z->pend + z->pend_len); z->pend_len += 4; }
            z->finished = true;
            continue;
        }
        if (z->fill > z->pos) {
            e = emit(z, z->fill, false);
            if (e != PROVEN_OK) break;
            continue;
        }
        break;
    }
    if (e != PROVEN_OK) {
        if (e == PROVEN_ERR_NOMEM) z->failed = true;
        return e;
    }
    *consumed = in_at;
    *produced = out_at;
    *done = z->finished && z->pend_at == z->pend_len;
    return PROVEN_OK;
}

proven_size_t proven_zstd_encode_bound(proven_size_t n) {
    /* Stored blocks are the worst case: three bytes for each of at least 1 KiB, the last one
     * excepted; then the frame header (at most 18 bytes, with a dictionary's ID) and the checksum. */
    const proven_size_t extra = (n >> 8) + 32;
    return n > (proven_size_t)-1 - extra ? (proven_size_t)-1 : n + extra;
}

proven_err_t proven_zstd_encode_all(const proven_zstd_encoder_options_t *options, proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    if (written) *written = 0;
    if (!written || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_zstd_encoder_t *z = NULL;
    /* Positions are 32 bits: a larger input goes through the buffer, a window at a time. */
    const bool direct = in.size < ((proven_size_t)1 << 31);
    proven_err_t e = make(options, direct ? (proven_u64)in.size : ~(proven_u64)0, &z);
    if (e != PROVEN_OK) return e;
    if (!direct) {
        z->stated = in.size;           /* the whole input is here: the frame states its size as below */
        proven_size_t used = 0, made = 0, in_at = 0, out_at = 0;
        bool done = false;
        while (e == PROVEN_OK && !done) {
            e = proven_zstd_encode(z, (proven_mem_view_t){ .ptr = in.ptr + in_at, .size = in.size - in_at }, &used,
                                   (proven_mem_mut_t){ .ptr = out.ptr + out_at, .size = out.size - out_at }, &made, PROVEN_ZSTD_FLUSH_FINISH, &done);
            in_at += used; out_at += made;
            if (e == PROVEN_OK && !done && used == 0 && made == 0) e = PROVEN_ERR_OUT_OF_BOUNDS;
        }
        proven_zstd_encoder_destroy(z);
        if (e == PROVEN_OK) *written = out_at;
        return e;
    }
    /* In place: the input is the window, and blocks go straight to `out` while there is room
     * for the worst case, through `pend` when there is not. */
    proven_size_t o = 0;
    const proven_size_t n = in.size;
    proven_byte_t hdr[18];
    const proven_size_t hl = frame_header(hdr, z->wlog, z->opt.checksum, true, n, z->cd ? z->cd->d.id : 0);
    if (out.size < hl) e = PROVEN_ERR_OUT_OF_BOUNDS;
    else { (void)proven_mem_copy(out.ptr, hl, (proven_mem_view_t){ .ptr = hdr, .size = hl }); o = hl; }
    if (e == PROVEN_OK && z->opt.checksum && n) xxh_update(&z->xxh, in.ptr, n);
    if (e == PROVEN_OK && n == 0) {
        if (out.size - o < 3) e = PROVEN_ERR_OUT_OF_BOUNDS;
        else { out.ptr[o] = 1; out.ptr[o + 1] = 0; out.ptr[o + 2] = 0; o += 3; }
    }
    if (e == PROVEN_OK && n > 0) e = block_room(z);
    /* With a dictionary the input follows its content in one buffer: matches reach back into it. */
    const proven_byte_t *base = in.ptr;
    proven_size_t pos = 0, end = n;
    if (e == PROVEN_OK && n > 0 && z->cd) {
        const proven_size_t dn = z->cd->d.content_len;
        e = grow(z, (void **)&z->buf, &z->buf_cap, dn + n, 1);
        if (e == PROVEN_OK) {
            (void)proven_mem_copy(z->buf, dn, (proven_mem_view_t){ .ptr = z->cd->d.content, .size = dn });
            (void)proven_mem_copy(z->buf + dn, n, (proven_mem_view_t){ .ptr = in.ptr, .size = n });
            base = z->buf;
            pos = dn;
            end = dn + n;
            dict_load(z, base, dn);
        }
    }
    while (e == PROVEN_OK && pos < end) {
        const proven_size_t to = end - pos > z->block_size ? pos + z->block_size : end;
        proven_size_t ended = to, k;
        if (out.size - o >= block_out_max(z)) {
            k = encode_block(z, base, pos, to, to == end, out.ptr + o, out.size - o, &ended);
        } else {
            e = grow(z, (void **)&z->pend, &z->pend_cap, block_out_max(z), 1);
            if (e != PROVEN_OK) break;
            k = encode_block(z, base, pos, to, to == end, z->pend, z->pend_cap, &ended);
            if (k > out.size - o) { e = PROVEN_ERR_OUT_OF_BOUNDS; break; }
            (void)proven_mem_copy(out.ptr + o, k, (proven_mem_view_t){ .ptr = z->pend, .size = k });
        }
        if (k == 0) { e = PROVEN_ERR_OUT_OF_BOUNDS; break; }
        o += k;
        pos = ended;
    }
    if (e == PROVEN_OK && z->opt.checksum) {
        if (out.size - o < 4) e = PROVEN_ERR_OUT_OF_BOUNDS;
        else { put_check(z, out.ptr + o); o += 4; }
    }
    proven_zstd_encoder_destroy(z);
    if (e == PROVEN_OK) *written = o;
    return e;
}
