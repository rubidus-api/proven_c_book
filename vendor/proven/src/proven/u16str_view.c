#include "proven/u16str.h"

#ifndef PROVEN_NO_U16STR

/*
 * The 16-bit view vocabulary (RFC-0044): u8str.c's view rules in units. Plain loops - no platform
 * call, so it is freestanding as it stands and needs no memcmp of a type it does not own.
 */

typedef proven_u16str_view_t view_t;

static view_t view_or_empty(view_t s) {
    return (s.size > 0 && !s.ptr) ? (view_t){ (const proven_u16 *)0, 0 } : s;
}

/* The one spelling of empty: {NULL, 0}. */
static view_t view_sub(view_t s, proven_size_t from, proven_size_t to) {
    if (to <= from) return (view_t){ (const proven_u16 *)0, 0 };
    return (view_t){ s.ptr + from, to - from };
}

static bool units_same(const proven_u16 *a, const proven_u16 *b, proven_size_t n) {
    for (proven_size_t i = 0; i < n; ++i) if (a[i] != b[i]) return false;
    return true;
}

static bool units_same_ci(const proven_u16 *a, const proven_u16 *b, proven_size_t n) {
    for (proven_size_t i = 0; i < n; ++i) if (proven_u16_ascii_lower(a[i]) != proven_u16_ascii_lower(b[i])) return false;
    return true;
}

proven_size_t proven_u16_cstr_len(const proven_u16 *s) {
    proven_size_t n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

int proven_u16str_view_eq(view_t a, view_t b) {
    a = view_or_empty(a);
    b = view_or_empty(b);
    return a.size == b.size && (a.size == 0 || units_same(a.ptr, b.ptr, a.size));
}

static int cmp_by(view_t a, view_t b, bool ci) {
    a = view_or_empty(a);
    b = view_or_empty(b);
    proven_size_t n = a.size < b.size ? a.size : b.size;
    for (proven_size_t i = 0; i < n; ++i) {
        proven_u16 x = ci ? proven_u16_ascii_lower(a.ptr[i]) : a.ptr[i];
        proven_u16 y = ci ? proven_u16_ascii_lower(b.ptr[i]) : b.ptr[i];
        if (x != y) return x < y ? -1 : 1;
    }
    return (a.size > b.size) - (a.size < b.size);
}

int proven_u16str_view_cmp(view_t a, view_t b) { return cmp_by(a, b, false); }
int proven_u16str_view_cmp_ascii_ci(view_t a, view_t b) { return cmp_by(a, b, true); }

int proven_u16str_view_eq_ascii_ci(view_t a, view_t b) {
    a = view_or_empty(a);
    b = view_or_empty(b);
    return a.size == b.size && (a.size == 0 || units_same_ci(a.ptr, b.ptr, a.size));
}

static int edge_match(view_t s, view_t part, bool at_end, bool ci) {
    s = view_or_empty(s);
    part = view_or_empty(part);
    if (part.size == 0) return 1;
    if (s.size < part.size) return 0;
    const proven_u16 *from = at_end ? s.ptr + (s.size - part.size) : s.ptr;
    return ci ? units_same_ci(from, part.ptr, part.size) : units_same(from, part.ptr, part.size);
}

int proven_u16str_view_starts_with(view_t s, view_t p) { return edge_match(s, p, false, false); }
int proven_u16str_view_ends_with(view_t s, view_t p) { return edge_match(s, p, true, false); }
int proven_u16str_view_starts_with_ascii_ci(view_t s, view_t p) { return edge_match(s, p, false, true); }
int proven_u16str_view_ends_with_ascii_ci(view_t s, view_t p) { return edge_match(s, p, true, true); }

proven_size_t proven_u16str_view_find_unit(view_t h, proven_size_t start, proven_u16 unit) {
    h = view_or_empty(h);
    for (proven_size_t i = start; i < h.size; ++i) if (h.ptr[i] == unit) return i;
    return PROVEN_INDEX_NOT_FOUND;
}

proven_size_t proven_u16str_view_find_last_unit(view_t h, proven_u16 unit) {
    h = view_or_empty(h);
    for (proven_size_t i = h.size; i-- > 0;) if (h.ptr[i] == unit) return i;
    return PROVEN_INDEX_NOT_FOUND;
}

proven_size_t proven_u16str_view_find(view_t h, proven_size_t start, view_t needle) {
    h = view_or_empty(h);
    needle = view_or_empty(needle);
    if (start > h.size) return PROVEN_INDEX_NOT_FOUND;
    if (needle.size == 0) return start;
    if (needle.size > h.size - start) return PROVEN_INDEX_NOT_FOUND;
    const proven_size_t last = h.size - needle.size;
    for (proven_size_t i = start; i <= last; ++i) {
        if (h.ptr[i] == needle.ptr[0] && units_same(h.ptr + i + 1, needle.ptr + 1, needle.size - 1)) return i;
    }
    return PROVEN_INDEX_NOT_FOUND;
}

proven_size_t proven_u16str_view_find_last(view_t h, view_t needle) {
    h = view_or_empty(h);
    needle = view_or_empty(needle);
    if (needle.size == 0) return h.size;
    if (needle.size > h.size) return PROVEN_INDEX_NOT_FOUND;
    for (proven_size_t i = h.size - needle.size + 1; i-- > 0;) {
        if (h.ptr[i] == needle.ptr[0] && units_same(h.ptr + i + 1, needle.ptr + 1, needle.size - 1)) return i;
    }
    return PROVEN_INDEX_NOT_FOUND;
}

bool proven_u16str_view_contains(view_t h, view_t needle) {
    return proven_u16str_view_find(h, 0, needle) != PROVEN_INDEX_NOT_FOUND;
}

view_t proven_u16str_view_slice(view_t s, proven_size_t index, proven_size_t len) {
    s = view_or_empty(s);
    if (index >= s.size || len == 0) return (view_t){ (const proven_u16 *)0, 0 };
    proven_size_t end = len > s.size - index ? s.size : index + len;
    return view_sub(s, index, end);
}

static bool unit_is_space(proven_u16 c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

view_t proven_u16str_view_trim_start(view_t s) {
    s = view_or_empty(s);
    proven_size_t i = 0;
    while (i < s.size && unit_is_space(s.ptr[i])) ++i;
    return view_sub(s, i, s.size);
}

view_t proven_u16str_view_trim_end(view_t s) {
    s = view_or_empty(s);
    proven_size_t end = s.size;
    while (end > 0 && unit_is_space(s.ptr[end - 1])) --end;
    return view_sub(s, 0, end);
}

view_t proven_u16str_view_trim(view_t s) {
    return proven_u16str_view_trim_end(proven_u16str_view_trim_start(s));
}

view_t proven_u16str_view_remove_prefix(view_t s, view_t prefix) {
    s = view_or_empty(s);
    prefix = view_or_empty(prefix);
    if (!proven_u16str_view_starts_with(s, prefix)) return s;
    return view_sub(s, prefix.size, s.size);
}

view_t proven_u16str_view_remove_suffix(view_t s, view_t suffix) {
    s = view_or_empty(s);
    suffix = view_or_empty(suffix);
    if (!proven_u16str_view_ends_with(s, suffix)) return s;
    return view_sub(s, 0, s.size - suffix.size);
}

proven_u16str_view_split_t proven_u16str_view_split(view_t src, view_t sep) {
    return (proven_u16str_view_split_t){ .rest = view_or_empty(src), .sep = view_or_empty(sep), .done = false };
}

/* u8str.c's four steps (RFC-0005 section 3.1), in the same order and for the same reasons. */
bool proven_u16str_view_split_next(proven_u16str_view_split_t *it, view_t *out) {
    if (!it || !out || it->done) return false;
    if (it->sep.size == 0) {
        *out = it->rest;
        it->done = true;
        return true;
    }
    proven_size_t at = proven_u16str_view_find(it->rest, 0, it->sep);
    if (at == PROVEN_INDEX_NOT_FOUND) {
        *out = it->rest;
        it->done = true;
        return true;
    }
    *out = view_sub(it->rest, 0, at);
    it->rest = view_sub(it->rest, at + it->sep.size, it->rest.size);
    return true;
}

proven_u16str_view_tokens_t proven_u16str_view_tokens(view_t src, view_t delims) {
    return (proven_u16str_view_tokens_t){ .rest = view_or_empty(src), .delims = view_or_empty(delims) };
}

static bool is_delim(const proven_u16str_view_tokens_t *it, proven_u16 c) {
    return proven_u16str_view_find_unit(it->delims, 0, c) != PROVEN_INDEX_NOT_FOUND;
}

bool proven_u16str_view_tokens_next(proven_u16str_view_tokens_t *it, view_t *out) {
    if (!it || !out) return false;
    view_t r = view_or_empty(it->rest);
    proven_size_t i = 0;
    while (i < r.size && is_delim(it, r.ptr[i])) ++i;
    if (i == r.size) {
        it->rest = (view_t){ (const proven_u16 *)0, 0 };
        return false;
    }
    proven_size_t j = i;
    while (j < r.size && !is_delim(it, r.ptr[j])) ++j;
    *out = view_sub(r, i, j);
    it->rest = view_sub(r, j, r.size);
    return true;
}

static int digit_of(proven_u16 c, unsigned base) {
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (base == 16 && c >= 'a' && c <= 'f') return (int)(c - 'a') + 10;
    if (base == 16 && c >= 'A' && c <= 'F') return (int)(c - 'A') + 10;
    return -1;
}

/* The magnitude after an optional sign, at most `limit`. */
static proven_err_t parse_mag(view_t s, unsigned base, bool sign_ok, proven_size_t *used, proven_u64 limit_pos,
                              proven_u64 limit_neg, bool *negative, proven_u64 *mag) {
    if ((base != 10 && base != 16) || !mag) return PROVEN_ERR_INVALID_ARG;
    s = view_or_empty(s);
    proven_size_t i = 0;
    *negative = false;
    if (sign_ok && i < s.size && (s.ptr[i] == '+' || s.ptr[i] == '-')) {
        *negative = s.ptr[i] == '-';
        ++i;
    }
    if (base == 16 && i + 2 < s.size && s.ptr[i] == '0' && (s.ptr[i + 1] == 'x' || s.ptr[i + 1] == 'X') &&
        digit_of(s.ptr[i + 2], 16) >= 0) {
        i += 2;
    }
    const proven_size_t first = i;
    const proven_u64 limit = *negative ? limit_neg : limit_pos;
    proven_u64 v = 0;
    bool over = false;
    for (int d; i < s.size && (d = digit_of(s.ptr[i], base)) >= 0; ++i) {
        if (v > (limit - (proven_u64)d) / base) over = true;
        else v = v * base + (proven_u64)d;
    }
    if (i == first) return PROVEN_ERR_INVALID_ARG;
    if (!used && i != s.size) return PROVEN_ERR_INVALID_ARG;
    if (over) return PROVEN_ERR_OVERFLOW;
    if (used) *used = i;
    *mag = v;
    return PROVEN_OK;
}

proven_err_t proven_u16str_view_parse_u64(view_t s, unsigned base, proven_size_t *used, proven_u64 *out) {
    if (!out) return PROVEN_ERR_INVALID_ARG;
    bool neg;
    proven_u64 m;
    proven_err_t e = parse_mag(s, base, false, used, UINT64_MAX, 0, &neg, &m);
    if (e == PROVEN_OK) *out = m;
    return e;
}

proven_err_t proven_u16str_view_parse_u32(view_t s, unsigned base, proven_size_t *used, proven_u32 *out) {
    if (!out) return PROVEN_ERR_INVALID_ARG;
    bool neg;
    proven_u64 m;
    proven_err_t e = parse_mag(s, base, false, used, UINT32_MAX, 0, &neg, &m);
    if (e == PROVEN_OK) *out = (proven_u32)m;
    return e;
}

proven_err_t proven_u16str_view_parse_i64(view_t s, unsigned base, proven_size_t *used, proven_i64 *out) {
    if (!out) return PROVEN_ERR_INVALID_ARG;
    bool neg;
    proven_u64 m;
    proven_err_t e = parse_mag(s, base, true, used, (proven_u64)INT64_MAX, (proven_u64)INT64_MAX + 1u, &neg, &m);
    if (e == PROVEN_OK) *out = neg ? (m == (proven_u64)INT64_MAX + 1u ? INT64_MIN : -(proven_i64)m) : (proven_i64)m;
    return e;
}

proven_err_t proven_u16str_view_parse_i32(view_t s, unsigned base, proven_size_t *used, proven_i32 *out) {
    if (!out) return PROVEN_ERR_INVALID_ARG;
    bool neg;
    proven_u64 m;
    proven_err_t e = parse_mag(s, base, true, used, (proven_u64)INT32_MAX, (proven_u64)INT32_MAX + 1u, &neg, &m);
    if (e == PROVEN_OK) *out = neg ? (proven_i32)(-(proven_i64)m) : (proven_i32)m;
    return e;
}

proven_result_size_t proven_u16str_view_copy_to(proven_u16 *buf, proven_size_t cap, view_t view) {
    view = view_or_empty(view);
    if (cap == 0) return (proven_result_size_t){ .err = PROVEN_ERR_OUT_OF_BOUNDS, .value = 0 };
    if (!buf) return (proven_result_size_t){ .err = PROVEN_ERR_INVALID_ARG, .value = 0 };
    proven_size_t n = view.size < cap - 1 ? view.size : cap - 1;
    for (proven_size_t i = 0; i < n; ++i) buf[i] = view.ptr[i];
    buf[n] = 0;
    return (proven_result_size_t){ .err = n == view.size ? PROVEN_OK : PROVEN_ERR_OUT_OF_BOUNDS, .value = n };
}

#endif /* PROVEN_NO_U16STR */
