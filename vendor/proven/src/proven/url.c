#include "proven/url.h"
#include "proven/utf.h"

/*
 * RFC 3986, for URLs that have an authority. Plain byte code throughout: no C library call and
 * no allocation, so this unit is part of the freestanding profile.
 */

static bool url_is_alpha(proven_byte_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool url_is_digit(proven_byte_t c) { return c >= '0' && c <= '9'; }

static int url_hex(proven_byte_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}



static proven_u8str_view_t url_sub(proven_u8str_view_t s, proven_size_t from, proven_size_t to) {
    return (proven_u8str_view_t){ .ptr = s.ptr + from, .size = to - from };
}

/* Printable ASCII only, and every '%' followed by two hex digits. */
static bool url_text_ok(proven_u8str_view_t s) {
    for (proven_size_t i = 0; i < s.size; ++i) {
        proven_byte_t c = s.ptr[i];
        if (c <= 0x20 || c >= 0x7f) return false;
        if (c == '%') {
            if (s.size - i < 3) return false;
            if (url_hex(s.ptr[i + 1]) < 0 || url_hex(s.ptr[i + 2]) < 0) return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// Taking a URL apart
// -----------------------------------------------------------------------------

proven_err_t proven_url_parse(proven_u8str_view_t text, proven_url_t *out) {
    if (!out || (text.size > 0 && !text.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (text.size == 0 || !url_text_ok(text)) return PROVEN_ERR_INVALID_FORMAT;

    proven_url_t u = {0};
    const proven_byte_t *p = text.ptr;
    proven_size_t n = text.size;

    /* scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ), then "://" */
    proven_size_t i = 0;
    if (!url_is_alpha(p[0])) return PROVEN_ERR_INVALID_FORMAT;
    while (i < n && (url_is_alpha(p[i]) || url_is_digit(p[i]) || p[i] == '+' || p[i] == '-' || p[i] == '.')) i++;
    if (i + 2 >= n || p[i] != ':' || p[i + 1] != '/' || p[i + 2] != '/') return PROVEN_ERR_INVALID_FORMAT;
    u.scheme = url_sub(text, 0, i);
    i += 3;

    /* The authority runs to the first '/', '?' or '#'. */
    proven_size_t auth_start = i;
    while (i < n && p[i] != '/' && p[i] != '?' && p[i] != '#') i++;
    proven_size_t auth_end = i;

    /* userinfo ends at the LAST '@' of the authority: "user@evil@host" has the host "host". */
    proven_size_t host_start = auth_start;
    for (proven_size_t k = auth_end; k > auth_start; --k) {
        if (p[k - 1] == '@') {
            u.userinfo = url_sub(text, auth_start, k - 1);
            u.has_userinfo = true;
            host_start = k;
            break;
        }
    }

    proven_size_t host_end = auth_end;
    proven_size_t port_start = auth_end;       /* == auth_end: no port */
    if (host_start < auth_end && p[host_start] == '[') {
        proven_size_t close = host_start + 1;
        while (close < auth_end && p[close] != ']') close++;
        if (close == auth_end || close == host_start + 1) return PROVEN_ERR_INVALID_FORMAT;
        /* Shape only: hex digits, colons and dots, then optionally a zone - "%25" (an encoded
         * '%') followed by unreserved characters, as RFC 6874 writes it. */
        bool zone = false;
        for (proven_size_t k = host_start + 1; k < close; ++k) {
            proven_byte_t c = p[k];
            if (!zone) {
                if (url_hex(c) >= 0 || c == ':' || c == '.') continue;
                if (c == '%' && close - k > 3 && p[k + 1] == '2' && p[k + 2] == '5') { zone = true; k += 2; continue; }
                return PROVEN_ERR_INVALID_FORMAT;
            }
            if (!(url_is_alpha(c) || url_is_digit(c) || c == '-' || c == '.' || c == '_' || c == '~')) return PROVEN_ERR_INVALID_FORMAT;
        }
        u.host = url_sub(text, host_start + 1, close);
        u.host_is_ipv6 = true;
        host_end = close + 1;
        if (host_end < auth_end) {
            if (p[host_end] != ':') return PROVEN_ERR_INVALID_FORMAT;
            port_start = host_end + 1;
        }
    } else {
        for (proven_size_t k = host_start; k < auth_end; ++k) {
            if (p[k] == ':') { host_end = k; port_start = k + 1; break; }
            /* '[' and ']' belong to an IP literal only; '@' cannot occur here any more. */
            if (p[k] == '[' || p[k] == ']') return PROVEN_ERR_INVALID_FORMAT;
        }
        u.host = url_sub(text, host_start, host_end);
    }

    if (port_start < auth_end) {
        proven_u32 port = 0;
        for (proven_size_t k = port_start; k < auth_end; ++k) {
            if (!url_is_digit(p[k])) return PROVEN_ERR_INVALID_FORMAT;
            port = port * 10u + (proven_u32)(p[k] - '0');
            if (port > 65535u) return PROVEN_ERR_INVALID_FORMAT;
        }
        u.port = (proven_u16)port;
        u.has_port = true;
    }
    /* "http://:80/" and "http://user@/": a port or a user with nowhere to go. */
    if (u.host.size == 0 && (u.has_port || u.has_userinfo)) return PROVEN_ERR_INVALID_FORMAT;

    proven_size_t path_start = i;
    while (i < n && p[i] != '?' && p[i] != '#') i++;
    u.path = url_sub(text, path_start, i);
    if (i < n && p[i] == '?') {
        proven_size_t q = ++i;
        while (i < n && p[i] != '#') i++;
        u.query = url_sub(text, q, i);
        u.has_query = true;
    }
    if (i < n && p[i] == '#') {
        u.fragment = url_sub(text, i + 1, n);
        u.has_fragment = true;
    }
    *out = u;
    return PROVEN_OK;
}

proven_u16 proven_url_default_port(proven_u8str_view_t scheme) {
    if (proven_u8str_view_eq_ascii_ci(scheme, PROVEN_LIT("http")) || proven_u8str_view_eq_ascii_ci(scheme, PROVEN_LIT("ws"))) return 80;
    if (proven_u8str_view_eq_ascii_ci(scheme, PROVEN_LIT("https")) || proven_u8str_view_eq_ascii_ci(scheme, PROVEN_LIT("wss"))) return 443;
    if (proven_u8str_view_eq_ascii_ci(scheme, PROVEN_LIT("ftp"))) return 21;
    return 0;
}

proven_u16 proven_url_effective_port(const proven_url_t *url) {
    if (!url) return 0;
    return url->has_port ? url->port : proven_url_default_port(url->scheme);
}

bool proven_url_scheme_is(const proven_url_t *url, proven_u8str_view_t scheme) {
    return url && proven_u8str_view_eq_ascii_ci(url->scheme, scheme);
}

proven_err_t proven_url_split_target(proven_u8str_view_t target, proven_u8str_view_t *path,
                                     proven_u8str_view_t *query, bool *has_query) {
    if (!path || !query || !has_query || (target.size > 0 && !target.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (target.size == 0 || !url_text_ok(target)) return PROVEN_ERR_INVALID_FORMAT;

    if (target.size == 1 && target.ptr[0] == '*') {
        *path = target;
        *query = (proven_u8str_view_t){ .ptr = target.ptr + 1, .size = 0 };
        *has_query = false;
        return PROVEN_OK;
    }
    if (target.ptr[0] == '/') {
        proven_size_t i = 0;
        while (i < target.size && target.ptr[i] != '?') {
            if (target.ptr[i] == '#') return PROVEN_ERR_INVALID_FORMAT;   /* a fragment is never sent */
            i++;
        }
        *path = url_sub(target, 0, i);
        *has_query = i < target.size;
        *query = *has_query ? url_sub(target, i + 1, target.size) : url_sub(target, target.size, target.size);
        for (proven_size_t k = 0; k < query->size; ++k) {
            if (query->ptr[k] == '#') return PROVEN_ERR_INVALID_FORMAT;
        }
        return PROVEN_OK;
    }
    proven_url_t u;
    if (proven_url_parse(target, &u) != PROVEN_OK || u.has_fragment || u.host.size == 0) return PROVEN_ERR_INVALID_FORMAT;
    /* "http://host" with no path asks for the root. */
    *path = u.path.size > 0 ? u.path : PROVEN_LIT("/");
    *query = u.query;
    *has_query = u.has_query;
    return PROVEN_OK;
}

// -----------------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------------

proven_url_query_iter_t proven_url_query_iter(proven_u8str_view_t query) {
    return (proven_url_query_iter_t){ .rest = query };
}

bool proven_url_query_next(proven_url_query_iter_t *it, proven_u8str_view_t *name, proven_u8str_view_t *value) {
    if (!it || !name || !value) return false;
    while (it->rest.size > 0) {
        proven_size_t end = 0;
        while (end < it->rest.size && it->rest.ptr[end] != '&') end++;
        proven_u8str_view_t pair = url_sub(it->rest, 0, end);
        it->rest = end < it->rest.size ? url_sub(it->rest, end + 1, it->rest.size)
                                       : url_sub(it->rest, it->rest.size, it->rest.size);
        if (pair.size == 0) continue;
        proven_size_t eq = 0;
        while (eq < pair.size && pair.ptr[eq] != '=') eq++;
        *name = url_sub(pair, 0, eq);
        *value = eq < pair.size ? url_sub(pair, eq + 1, pair.size) : url_sub(pair, pair.size, pair.size);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Percent-coding
// -----------------------------------------------------------------------------

static proven_err_t url_decode(proven_u8str_view_t in, proven_mem_mut_t out, proven_size_t *written, bool plus_is_space) {
    if (written) *written = 0;
    if (!written || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_size_t w = 0;
    for (proven_size_t i = 0; i < in.size; ++i) {
        proven_byte_t c = in.ptr[i];
        if (c == '%') {
            if (in.size - i < 3) return PROVEN_ERR_INVALID_FORMAT;
            int hi = url_hex(in.ptr[i + 1]), lo = url_hex(in.ptr[i + 2]);
            if (hi < 0 || lo < 0) return PROVEN_ERR_INVALID_FORMAT;
            c = (proven_byte_t)((hi << 4) | lo);
            i += 2;
        } else if (c == '+' && plus_is_space) {
            c = ' ';
        }
        if (w >= out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
        /* w never passes i, so writing here cannot overtake the read position when the output
         * is the input buffer itself. */
        out.ptr[w++] = c;
    }
    *written = w;
    return PROVEN_OK;
}

proven_err_t proven_url_percent_decode(proven_u8str_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    return url_decode(in, out, written, false);
}

proven_err_t proven_url_form_decode(proven_u8str_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    return url_decode(in, out, written, true);
}

proven_size_t proven_url_encoded_size_max(proven_size_t n) {
    return n > PROVEN_SIZE_MAX / 3u ? PROVEN_SIZE_MAX : n * 3u;
}

typedef enum { URL_KEEP_COMPONENT, URL_KEEP_PATH, URL_KEEP_FORM } proven_url_keep_t;

static bool url_unreserved(proven_byte_t c) {
    return url_is_alpha(c) || url_is_digit(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

static proven_err_t url_encode(proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written, proven_url_keep_t keep) {
    static const char hex[] = "0123456789ABCDEF";
    if (written) *written = 0;
    if (!written || (in.size > 0 && !in.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (proven_url_encoded_size_max(in.size) == PROVEN_SIZE_MAX) return PROVEN_ERR_OVERFLOW;
    proven_size_t w = 0;
    for (proven_size_t i = 0; i < in.size; ++i) {
        proven_byte_t c = in.ptr[i];
        bool plain = url_unreserved(c) || (keep == URL_KEEP_PATH && c == '/');
        if (plain || (keep == URL_KEEP_FORM && c == ' ')) {
            if (w >= out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
            out.ptr[w++] = plain ? c : (proven_byte_t)'+';
        } else {
            if (out.size - w < 3) return PROVEN_ERR_OUT_OF_BOUNDS;
            out.ptr[w++] = '%';
            out.ptr[w++] = (proven_byte_t)hex[c >> 4];
            out.ptr[w++] = (proven_byte_t)hex[c & 0xf];
        }
    }
    *written = w;
    return PROVEN_OK;
}

proven_err_t proven_url_encode_component(proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    return url_encode(in, out, written, URL_KEEP_COMPONENT);
}

proven_err_t proven_url_encode_path(proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    return url_encode(in, out, written, URL_KEEP_PATH);
}

proven_err_t proven_url_form_encode(proven_mem_view_t in, proven_mem_mut_t out, proven_size_t *written) {
    return url_encode(in, out, written, URL_KEEP_FORM);
}

proven_err_t proven_url_form_append(proven_mem_mut_t out, proven_size_t *len,
                                    proven_mem_view_t name, proven_mem_view_t value) {
    if (!len || *len > out.size || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_size_t at = *len;
    if (at > 0) {
        if (at >= out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
        out.ptr[at++] = '&';
    }
    proven_size_t n = 0;
    proven_err_t e = url_encode(name, (proven_mem_mut_t){ .ptr = out.ptr + at, .size = out.size - at }, &n, URL_KEEP_FORM);
    if (e != PROVEN_OK) return e;
    at += n;
    if (at >= out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
    out.ptr[at++] = '=';
    e = url_encode(value, (proven_mem_mut_t){ .ptr = out.ptr + at, .size = out.size - at }, &n, URL_KEEP_FORM);
    if (e != PROVEN_OK) return e;
    *len = at + n;
    return PROVEN_OK;
}

// -----------------------------------------------------------------------------
// Resolving a reference
// -----------------------------------------------------------------------------

typedef struct {
    proven_byte_t *ptr;
    proven_size_t cap;
    proven_size_t len;
    bool full;
} proven_url_out_t;

static void url_out_put(proven_url_out_t *o, proven_u8str_view_t s) {
    if (o->full || s.size > o->cap - o->len) { o->full = true; return; }
    for (proven_size_t i = 0; i < s.size; ++i) o->ptr[o->len + i] = s.ptr[i];
    o->len += s.size;
}

static void url_out_byte(proven_url_out_t *o, proven_byte_t c) {
    url_out_put(o, (proven_u8str_view_t){ .ptr = &c, .size = 1 });
}

/* RFC 3986 section 5.2.4, applied in place to o->ptr[from..o->len): "." segments vanish, ".."
 * removes the segment before it, and one that has nothing to remove is dropped - the path
 * stays at the root. This is the textual algorithm for building a URL, not the security
 * check for using a path, which is proven_url_path_resolve. */
static void url_remove_dots(proven_url_out_t *o, proven_size_t from) {
    proven_byte_t *p = o->ptr;
    proven_size_t n = o->len;
    if (from >= n || p[from] != '/') return;
    proven_size_t w = from + 1;
    proven_size_t r = from + 1;
    for (;;) {
        proven_size_t start = r;
        while (r < n && p[r] != '/') r++;
        proven_size_t len = r - start;
        bool last = r == n;
        bool dot = len == 1 && p[start] == '.';
        bool dotdot = len == 2 && p[start] == '.' && p[start + 1] == '.';
        if (dotdot) {
            if (w > from + 1) {
                w--;
                while (w > from + 1 && p[w - 1] != '/') w--;
            }
        } else if (!dot) {
            for (proven_size_t k = 0; k < len; ++k) p[w + k] = p[start + k];
            w += len;
            if (!last) p[w++] = '/';
        }
        if (last) break;
        r++;
    }
    o->len = w;
}

proven_err_t proven_url_resolve(proven_u8str_view_t base, proven_u8str_view_t reference,
                                proven_mem_mut_t out, proven_size_t *written) {
    if (written) *written = 0;
    if (!written || (reference.size > 0 && !reference.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_url_t b;
    if (proven_url_parse(base, &b) != PROVEN_OK) return PROVEN_ERR_INVALID_FORMAT;
    if (reference.size > 0 && !url_text_ok(reference)) return PROVEN_ERR_INVALID_FORMAT;

    proven_url_out_t o = { .ptr = out.ptr, .cap = out.size, .len = 0, .full = false };
    const proven_byte_t *r = reference.ptr;
    proven_size_t n = reference.size;

    /* A reference that has a scheme is already absolute: "scheme:" before any '/', '?' or '#'. */
    proven_size_t colon = 0;
    bool has_scheme = false;
    if (n > 0 && url_is_alpha(r[0])) {
        while (colon < n && (url_is_alpha(r[colon]) || url_is_digit(r[colon]) || r[colon] == '+' || r[colon] == '-' || r[colon] == '.')) colon++;
        has_scheme = colon < n && r[colon] == ':';
    }

    proven_size_t path_from = 0;
    if (has_scheme) {
        /* Already absolute: it is the answer, provided it is a URL this library takes. */
        url_out_put(&o, reference);
        proven_url_t abs;
        if (o.full) return PROVEN_ERR_OUT_OF_BOUNDS;
        if (proven_url_parse((proven_u8str_view_t){ .ptr = o.ptr, .size = o.len }, &abs) != PROVEN_OK) return PROVEN_ERR_INVALID_FORMAT;
        *written = o.len;
        return PROVEN_OK;
    }

    /* Split the reference into path, query and fragment. */
    proven_size_t q = 0;
    while (q < n && r[q] != '?' && r[q] != '#') q++;
    proven_size_t f = q;
    while (f < n && r[f] != '#') f++;
    proven_u8str_view_t rpath = { .ptr = r, .size = q };
    proven_u8str_view_t rquery = (q < n && r[q] == '?') ? (proven_u8str_view_t){ .ptr = r + q, .size = f - q } : (proven_u8str_view_t){ .ptr = r + q, .size = 0 };
    proven_u8str_view_t rfrag = { .ptr = r + f, .size = n - f };

    url_out_put(&o, b.scheme);
    url_out_byte(&o, ':');

    if (rpath.size >= 2 && rpath.ptr[0] == '/' && rpath.ptr[1] == '/') {
        /* "//host/path": everything but the scheme comes from the reference. */
        url_out_put(&o, reference);
        if (o.full) return PROVEN_ERR_OUT_OF_BOUNDS;
        proven_url_t abs;
        if (proven_url_parse((proven_u8str_view_t){ .ptr = o.ptr, .size = o.len }, &abs) != PROVEN_OK) return PROVEN_ERR_INVALID_FORMAT;
        *written = o.len;
        return PROVEN_OK;
    }

    /* The base's authority, exactly as it was written: from after "://" to the path. */
    url_out_byte(&o, '/');
    url_out_byte(&o, '/');
    const proven_byte_t *auth = b.scheme.ptr + b.scheme.size + 3;
    url_out_put(&o, (proven_u8str_view_t){ .ptr = auth, .size = (proven_size_t)(b.path.ptr - auth) });
    path_from = o.len;

    if (rpath.size == 0) {
        url_out_put(&o, b.path);
        if (rquery.size > 0) url_out_put(&o, rquery);
        else if (b.has_query) { url_out_byte(&o, '?'); url_out_put(&o, b.query); }
    } else {
        if (rpath.ptr[0] == '/') {
            url_out_put(&o, rpath);
        } else {
            /* Merge: the base path up to and including its last '/', then the reference. */
            proven_size_t cut = b.path.size;
            while (cut > 0 && b.path.ptr[cut - 1] != '/') cut--;
            if (cut == 0) url_out_byte(&o, '/');
            else url_out_put(&o, (proven_u8str_view_t){ .ptr = b.path.ptr, .size = cut });
            url_out_put(&o, rpath);
        }
        if (!o.full) url_remove_dots(&o, path_from);
        url_out_put(&o, rquery);
    }
    url_out_put(&o, rfrag);
    if (o.full) return PROVEN_ERR_OUT_OF_BOUNDS;
    *written = o.len;
    return PROVEN_OK;
}

// -----------------------------------------------------------------------------
// A path that is safe to use
// -----------------------------------------------------------------------------

proven_err_t proven_url_path_resolve(proven_u8str_view_t raw_path, proven_mem_mut_t out, proven_size_t *written) {
    if (written) *written = 0;
    if (!written || (raw_path.size > 0 && !raw_path.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (raw_path.size == 0 || raw_path.ptr[0] != '/') return PROVEN_ERR_INVALID_FORMAT;

    /* Step 1: decode once into `out`, refusing what must not be in a file name. An encoded
     * slash is refused, so afterwards every '/' in `out` is a separator that was written as
     * one - which is what lets step 2 trust them. */
    proven_size_t n = 0;
    for (proven_size_t i = 0; i < raw_path.size; ++i) {
        proven_byte_t c = raw_path.ptr[i];
        if (c == '%') {
            if (raw_path.size - i < 3) return PROVEN_ERR_INVALID_FORMAT;
            int hi = url_hex(raw_path.ptr[i + 1]), lo = url_hex(raw_path.ptr[i + 2]);
            if (hi < 0 || lo < 0) return PROVEN_ERR_INVALID_FORMAT;
            c = (proven_byte_t)((hi << 4) | lo);
            i += 2;
            if (c == '/') return PROVEN_ERR_INVALID_FORMAT;
        }
        if (c < 0x20 || c == 0x7f || c == '\\') return PROVEN_ERR_INVALID_FORMAT;
        if (n >= out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
        out.ptr[n++] = c;
    }

    /* Decoded bytes above ASCII must be UTF-8, which is what refuses an overlong encoding of a
     * dot: "%c0%ae" is not a character at all. Checked before the dot segments are resolved,
     * so nothing malformed takes part in that. */
    proven_u8str_view_t decoded = { .ptr = out.ptr, .size = n };
    for (proven_size_t pos = 0; pos < n;) {
        proven_utf8_char_t ch = proven_utf8_decode_next(decoded, pos);
        if (ch.err != PROVEN_OK || ch.len == 0) return PROVEN_ERR_INVALID_FORMAT;
        pos += ch.len;
    }

    /* Step 2: resolve the segments in place. out[0..w) is the resolved path so far and always
     * ends in '/', so "the segment before" is found by stepping back over one '/'. The write
     * position never passes the read position. */
    proven_size_t w = 1;                    /* the leading '/' stays where it is */
    proven_size_t r = 1;
    for (;;) {
        proven_size_t start = r;
        while (r < n && out.ptr[r] != '/') r++;
        proven_size_t len = r - start;
        bool last = r == n;
        bool dot = len == 1 && out.ptr[start] == '.';
        bool dotdot = len == 2 && out.ptr[start] == '.' && out.ptr[start + 1] == '.';

        if (dotdot) {
            if (w <= 1) return PROVEN_ERR_PERMISSION;       /* above the root */
            w--;                                           /* the '/' that ends the previous segment */
            while (w > 0 && out.ptr[w - 1] != '/') w--;    /* and the segment itself */
        } else if (len > 0 && !dot) {
            for (proven_size_t k = 0; k < len; ++k) out.ptr[w + k] = out.ptr[start + k];
            w += len;
            /* Followed by a separator, the name is a directory on the way; at the end it is
             * the thing asked for, and gets no slash it did not have. */
            if (!last) out.ptr[w++] = '/';
        }
        /* An empty segment ("//") and "." add nothing. When either - or ".." - is the last
         * segment, the path names a directory, and the '/' it ends in is already there. */
        if (last) break;
        r++;
    }
    *written = w;
    return PROVEN_OK;
}
