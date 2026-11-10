#include "proven/sign.h"
#include "proven/der.h"
#include "proven/cert.h"
#include "proven/hash.h"
#include "proven_internal_crypto.h"
#include <stdatomic.h>

/* sign.h: private keys, signing and verification over the crypto the TLS engine uses. */

#define PK_RSA_RENEW 65536u      /* signatures between fresh blinding pairs, when the caller gives a generator */

struct proven_pkey {
    proven_allocator_t alloc;
    proven_pkey_kind_t kind;
    proven_size_t bits;
    /* EC */
    proven_size_t ec_size;
    proven_byte_t ec_priv[PROVEN_CRYPTO_EC_MAX_BYTES];
    proven_byte_t ec_pub[1 + 2 * PROVEN_CRYPTO_EC_MAX_BYTES];
    /* RSA */
    proven_crypto_rsa_key_t rsa;
    proven_crypto_rsa_blind_t blind;
    atomic_flag lock;           /* held while the blinding pair moves: signing may run on several threads */
    proven_u32 uses;
    proven_size_t d_len;
    proven_byte_t d[PROVEN_CRYPTO_RSA_SIGN_MAX_BYTES];
};

static const proven_byte_t OID_RSA[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 };
static const proven_byte_t OID_EC[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01 };
static const proven_byte_t OID_P256[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07 };
static const proven_byte_t OID_P384[] = { 0x2b, 0x81, 0x04, 0x00, 0x22 };

#define PK_VIEW(a) ((proven_mem_view_t){ .ptr = (a), .size = sizeof (a) })

static void pk_wipe(void *p, proven_size_t n) { proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)p, .size = n }); }

static bool pk_ec_curve(proven_pkey_kind_t kind, proven_crypto_ec_curve_t *curve, proven_size_t *size) {
    if (kind == PROVEN_PKEY_P256) { *curve = PROVEN_CRYPTO_EC_P256; *size = 32; return true; }
    if (kind == PROVEN_PKEY_P384) { *curve = PROVEN_CRYPTO_EC_P384; *size = 48; return true; }
    return false;
}

static proven_pkey_kind_t pk_curve_of_oid(proven_mem_view_t oid) {
    if (proven_der_oid_is(oid, PK_VIEW(OID_P256))) return PROVEN_PKEY_P256;
    if (proven_der_oid_is(oid, PK_VIEW(OID_P384))) return PROVEN_PKEY_P384;
    return PROVEN_PKEY_NONE;
}

static proven_err_t pk_new(proven_allocator_t alloc, proven_pkey_kind_t kind, proven_pkey_t **out) {
    proven_result_mem_mut_t m = alloc.alloc_fn(alloc.ctx, sizeof(proven_pkey_t), 16);
    if (m.err != PROVEN_OK || !m.value.ptr) return PROVEN_ERR_NOMEM;
    proven_pkey_t *k = (proven_pkey_t *)(void *)m.value.ptr;
    for (proven_size_t i = 0; i < sizeof *k; ++i) ((proven_byte_t *)k)[i] = 0;
    k->alloc = alloc;
    k->kind = kind;
    atomic_flag_clear(&k->lock);
    *out = k;
    return PROVEN_OK;
}

void proven_pkey_destroy(proven_pkey_t *key) {
    if (!key) return;
    proven_allocator_t a = key->alloc;
    pk_wipe(key, sizeof *key);
    a.free_fn(a.ctx, key);
}

proven_pkey_kind_t proven_pkey_kind(const proven_pkey_t *key) { return key ? key->kind : PROVEN_PKEY_NONE; }
proven_size_t proven_pkey_bits(const proven_pkey_t *key) { return key ? key->bits : 0; }

/* ---- EC ---- */

static proven_err_t pk_ec_set(proven_pkey_t *k, proven_mem_view_t scalar) {
    proven_crypto_ec_curve_t curve;
    proven_size_t size;
    if (!pk_ec_curve(k->kind, &curve, &size)) return PROVEN_ERR_INVALID_ARG;
    /* Some writers drop leading zero bytes of the scalar: put them back. */
    if (scalar.size > size || scalar.size == 0) return PROVEN_ERR_INVALID_FORMAT;
    for (proven_size_t i = 0; i < size - scalar.size; ++i) k->ec_priv[i] = 0;
    for (proven_size_t i = 0; i < scalar.size; ++i) k->ec_priv[size - scalar.size + i] = scalar.ptr[i];
    if (!proven_crypto_ec_public(curve, k->ec_priv, k->ec_pub)) return PROVEN_ERR_INVALID_FORMAT;
    k->ec_size = size;
    k->bits = size * 8;
    return PROVEN_OK;
}

proven_err_t proven_pkey_from_ec(proven_allocator_t alloc, proven_pkey_kind_t curve, proven_mem_view_t scalar, proven_pkey_t **out) {
    if (out) *out = (void *)0;
    proven_crypto_ec_curve_t c;
    proven_size_t size;
    if (!out || !proven_alloc_is_valid(alloc) || !pk_ec_curve(curve, &c, &size) || !scalar.ptr || scalar.size != size) return PROVEN_ERR_INVALID_ARG;
    proven_pkey_t *k = (void *)0;
    proven_err_t e = pk_new(alloc, curve, &k);
    if (e != PROVEN_OK) return e;
    e = pk_ec_set(k, scalar);
    if (e != PROVEN_OK) { proven_pkey_destroy(k); return e; }
    *out = k;
    return PROVEN_OK;
}

/* ECPrivateKey (RFC 5915): SEQUENCE { 1, OCTET STRING key, [0] curve OPTIONAL, [1] BIT STRING point OPTIONAL }.
 * `curve` is what the PKCS #8 wrapper said, or NONE. */
static proven_err_t pk_sec1(proven_allocator_t alloc, proven_mem_view_t der, proven_pkey_kind_t curve, proven_pkey_t **out) {
    proven_mem_view_t in = der, seq, priv, f;
    proven_u64 version = 0;
    if (proven_der_expect(&in, PROVEN_DER_SEQUENCE, &seq) != PROVEN_OK || in.size != 0) return PROVEN_ERR_INVALID_FORMAT;
    if (proven_der_read_u64(&seq, &version) != PROVEN_OK || version != 1 ||
        proven_der_expect(&seq, PROVEN_DER_OCTET_STRING, &priv) != PROVEN_OK) return PROVEN_ERR_INVALID_FORMAT;
    if (proven_der_expect(&seq, PROVEN_DER_CONTEXT(0), &f) == PROVEN_OK) {
        proven_mem_view_t oid;
        if (proven_der_expect(&f, PROVEN_DER_OID, &oid) != PROVEN_OK || f.size != 0) return PROVEN_ERR_INVALID_FORMAT;
        const proven_pkey_kind_t named = pk_curve_of_oid(oid);
        if (named == PROVEN_PKEY_NONE) return PROVEN_ERR_UNSUPPORTED;
        if (curve != PROVEN_PKEY_NONE && curve != named) return PROVEN_ERR_INVALID_FORMAT;
        curve = named;
    }
    if (curve == PROVEN_PKEY_NONE) return PROVEN_ERR_INVALID_FORMAT;
    proven_mem_view_t point = { 0 };
    bool has_point = false;
    if (proven_der_expect(&seq, PROVEN_DER_CONTEXT(1), &f) == PROVEN_OK) {
        proven_u8 unused = 0;
        if (proven_der_read_bits(&f, &point, &unused) != PROVEN_OK || unused != 0 || f.size != 0) return PROVEN_ERR_INVALID_FORMAT;
        has_point = true;
    }
    if (seq.size != 0) return PROVEN_ERR_INVALID_FORMAT;
    proven_pkey_t *k = (void *)0;
    proven_err_t e = pk_new(alloc, curve, &k);
    if (e != PROVEN_OK) return e;
    e = pk_ec_set(k, priv);
    /* A public point written with the key must be the key's own. */
    if (e == PROVEN_OK && has_point) {
        const proven_size_t n = 1 + 2 * k->ec_size;
        bool same = point.size == n;
        for (proven_size_t i = 0; same && i < n; ++i) same = point.ptr[i] == k->ec_pub[i];
        if (!same) e = PROVEN_ERR_INVALID_FORMAT;
    }
    if (e != PROVEN_OK) { proven_pkey_destroy(k); return e; }
    *out = k;
    return PROVEN_OK;
}

/* ---- RSA ---- */

/* Make the blinding pair and sign once, checked with the public half. */
static proven_err_t pk_rsa_ready(proven_pkey_t *k, proven_rng_t rng) {
    const proven_crypto_rsa_key_t *r = &k->rsa;
    proven_byte_t seed[PROVEN_CRYPTO_RSA_SIGN_MAX_BYTES], digest[32], sig[PROVEN_CRYPTO_RSA_SIGN_MAX_BYTES];
    bool made = false;
    for (int attempt = 0; attempt < 4 && !made; ++attempt) {
        proven_rng_fill(rng, seed, r->n_len);
        made = proven_crypto_rsa_blind_make(r, seed, &k->blind);
    }
    pk_wipe(seed, sizeof seed);
    if (!made) return PROVEN_ERR_INVALID_FORMAT;
    proven_sha256((proven_mem_view_t){ .ptr = r->n, .size = r->n_len }, digest);
    proven_crypto_rsa_blind_t mine = k->blind;
    proven_crypto_rsa_blind_next(r, &k->blind);
    const bool ok = proven_crypto_rsa_sign_pkcs1(r, &mine, PROVEN_HMAC_SHA256, (proven_mem_view_t){ .ptr = digest, .size = 32 }, sig) &&
                    proven_crypto_rsa_verify_pkcs1((proven_mem_view_t){ .ptr = r->n, .size = r->n_len }, (proven_mem_view_t){ .ptr = r->e, .size = r->e_len },
                                                   PROVEN_HMAC_SHA256, (proven_mem_view_t){ .ptr = digest, .size = 32 },
                                                   (proven_mem_view_t){ .ptr = sig, .size = r->n_len });
    pk_wipe(&mine, sizeof mine);
    k->bits = r->n_len * 8;
    if (k->bits > 0) {
        /* The modulus's bits: its first byte need not be full. */
        proven_byte_t top = r->n[0];
        proven_size_t lead = 0;
        while (lead < 8 && !(top & (0x80u >> lead))) ++lead;
        k->bits -= lead;
    }
    return ok ? PROVEN_OK : PROVEN_ERR_INVALID_FORMAT;
}

static proven_err_t pk_rsa(proven_allocator_t alloc, proven_mem_view_t der, proven_mem_view_t d, proven_rng_t rng, proven_pkey_t **out) {
    if (!proven_rng_is_valid(rng)) return PROVEN_ERR_INVALID_ARG;
    /* What the parser refuses for size is said as unsupported: it reads 2048 to 4096 bits (a test
     * may lower the floor, as for verification). */
    {
        proven_mem_view_t in = der, seq, n;
        proven_u64 version;
        if (proven_der_expect(&in, PROVEN_DER_SEQUENCE, &seq) == PROVEN_OK && proven_der_read_u64(&seq, &version) == PROVEN_OK &&
            proven_der_read_uint(&seq, &n) == PROVEN_OK && (n.size * 8 < proven_crypto_rsa_min_bits() || n.size > PROVEN_CRYPTO_RSA_SIGN_MAX_BYTES))
            return PROVEN_ERR_UNSUPPORTED;
    }
    proven_pkey_t *k = (void *)0;
    proven_err_t e = pk_new(alloc, PROVEN_PKEY_RSA, &k);
    if (e != PROVEN_OK) return e;
    if (!proven_crypto_rsa_key_parse(der, &k->rsa)) { proven_pkey_destroy(k); return PROVEN_ERR_INVALID_FORMAT; }
    while (d.size > 0 && d.ptr[0] == 0) { d.ptr++; d.size--; }
    if (d.size > sizeof k->d) { proven_pkey_destroy(k); return PROVEN_ERR_INVALID_FORMAT; }
    for (proven_size_t i = 0; i < d.size; ++i) k->d[i] = d.ptr[i];
    k->d_len = d.size;
    e = pk_rsa_ready(k, rng);
    if (e != PROVEN_OK) { proven_pkey_destroy(k); return e; }
    *out = k;
    return PROVEN_OK;
}

/* d out of an RSAPrivateKey, for proven_pkey_rsa_components: the parser keeps only the CRT form. */
static proven_mem_view_t pk_rsa_d(proven_mem_view_t der) {
    proven_mem_view_t in = der, seq, v = { 0 };
    proven_u64 version;
    if (proven_der_expect(&in, PROVEN_DER_SEQUENCE, &seq) != PROVEN_OK || proven_der_read_u64(&seq, &version) != PROVEN_OK) return (proven_mem_view_t){ 0 };
    for (int i = 0; i < 3; ++i) if (proven_der_read_uint(&seq, &v) != PROVEN_OK) return (proven_mem_view_t){ 0 };
    return v;
}

proven_err_t proven_pkey_from_rsa(proven_allocator_t alloc, const proven_rsa_components_t *c, proven_rng_t rng, proven_pkey_t **out) {
    if (out) *out = (void *)0;
    if (!out || !c || !proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
    const proven_mem_view_t *parts[8] = { &c->n, &c->e, &c->d, &c->p, &c->q, &c->dp, &c->dq, &c->qinv };
    proven_size_t total = 64;
    for (int i = 0; i < 8; ++i) {
        if (parts[i]->size > 0 && !parts[i]->ptr) return PROVEN_ERR_INVALID_ARG;
        if (i != 2 && parts[i]->size == 0) return PROVEN_ERR_INVALID_ARG;
        if (parts[i]->size > 4096) return PROVEN_ERR_UNSUPPORTED;
        total += parts[i]->size + 8;
    }
    /* As an RSAPrivateKey, through the parser every other key goes through. */
    proven_result_mem_mut_t m = alloc.alloc_fn(alloc.ctx, total, 1);
    if (m.err != PROVEN_OK || !m.value.ptr) return PROVEN_ERR_NOMEM;
    proven_der_writer_t w;
    proven_der_writer_init(&w, (proven_mem_mut_t){ .ptr = m.value.ptr, .size = total });
    proven_der_begin(&w, PROVEN_DER_SEQUENCE);
    proven_der_put_u64(&w, 0);
    for (int i = 0; i < 8; ++i) proven_der_put_uint(&w, *parts[i]);
    proven_der_end(&w);
    proven_size_t len = 0;
    proven_err_t e = proven_der_writer_finish(&w, &len);
    if (e == PROVEN_OK) e = pk_rsa(alloc, (proven_mem_view_t){ .ptr = m.value.ptr, .size = len }, c->d, rng, out);
    pk_wipe(m.value.ptr, total);
    alloc.free_fn(alloc.ctx, m.value.ptr);
    return e;
}

/* ---- Reading a key ---- */

/* PrivateKeyInfo (RFC 5208 / 5958): SEQUENCE { version, AlgorithmIdentifier, OCTET STRING key, ... } */
static proven_err_t pk_pkcs8(proven_allocator_t alloc, proven_mem_view_t der, proven_rng_t rng, proven_pkey_t **out) {
    proven_mem_view_t in = der, seq, alg, oid, priv;
    proven_u64 version = 9;
    if (proven_der_expect(&in, PROVEN_DER_SEQUENCE, &seq) != PROVEN_OK || in.size != 0) return PROVEN_ERR_INVALID_FORMAT;
    if (proven_der_read_u64(&seq, &version) != PROVEN_OK || version > 1) return PROVEN_ERR_INVALID_FORMAT;
    if (proven_der_expect(&seq, PROVEN_DER_SEQUENCE, &alg) != PROVEN_OK || proven_der_expect(&alg, PROVEN_DER_OID, &oid) != PROVEN_OK ||
        proven_der_expect(&seq, PROVEN_DER_OCTET_STRING, &priv) != PROVEN_OK) return PROVEN_ERR_INVALID_FORMAT;
    if (proven_der_oid_is(oid, PK_VIEW(OID_RSA))) return pk_rsa(alloc, priv, pk_rsa_d(priv), rng, out);
    if (proven_der_oid_is(oid, PK_VIEW(OID_EC))) {
        proven_mem_view_t curve;
        if (proven_der_expect(&alg, PROVEN_DER_OID, &curve) != PROVEN_OK || alg.size != 0) return PROVEN_ERR_INVALID_FORMAT;
        const proven_pkey_kind_t kind = pk_curve_of_oid(curve);
        if (kind == PROVEN_PKEY_NONE) return PROVEN_ERR_UNSUPPORTED;
        return pk_sec1(alloc, priv, kind, out);
    }
    return PROVEN_ERR_UNSUPPORTED;
}

static bool pk_label(proven_u8str_view_t label, const char *want) {
    proven_size_t n = 0;
    while (want[n]) ++n;
    if (label.size != n) return false;
    for (proven_size_t i = 0; i < n; ++i) if (label.ptr[i] != (proven_byte_t)want[i]) return false;
    return true;
}

/* DER of one of the three, told apart by what follows the version: an AlgorithmIdentifier
 * (PKCS #8), the key's OCTET STRING (SEC1) or the modulus (PKCS #1). */
static proven_err_t pk_der(proven_allocator_t alloc, proven_mem_view_t der, proven_rng_t rng, proven_pkey_t **out) {
    proven_mem_view_t in = der, seq;
    proven_u64 version = 9;
    proven_der_item_t next;
    if (proven_der_expect(&in, PROVEN_DER_SEQUENCE, &seq) != PROVEN_OK || proven_der_read_u64(&seq, &version) != PROVEN_OK ||
        proven_der_read(&seq, &next) != PROVEN_OK) return PROVEN_ERR_INVALID_FORMAT;
    if (next.tag == PROVEN_DER_SEQUENCE) return pk_pkcs8(alloc, der, rng, out);
    if (next.tag == PROVEN_DER_OCTET_STRING) return pk_sec1(alloc, der, PROVEN_PKEY_NONE, out);
    if (next.tag == PROVEN_DER_INTEGER && version == 0) return pk_rsa(alloc, der, pk_rsa_d(der), rng, out);
    return PROVEN_ERR_INVALID_FORMAT;
}

proven_err_t proven_pkey_parse(proven_allocator_t alloc, proven_mem_view_t text, proven_rng_t rng, proven_pkey_t **out) {
    if (out) *out = (void *)0;
    if (!out || !proven_alloc_is_valid(alloc) || !text.ptr || text.size == 0) return PROVEN_ERR_INVALID_ARG;
    /* PEM when it has a BEGIN line; DER otherwise (0x30 is also '0', so the first byte cannot say). */
    static const char begin[] = "-----BEGIN";
    bool pem = false;
    for (proven_size_t i = 0; !pem && i + sizeof begin - 1 <= text.size; ++i) {
        proven_size_t k = 0;
        while (k < sizeof begin - 1 && text.ptr[i + k] == (proven_byte_t)begin[k]) ++k;
        pem = k == sizeof begin - 1;
    }
    if (!pem) return pk_der(alloc, text, rng, out);
    proven_result_mem_mut_t m = alloc.alloc_fn(alloc.ctx, text.size, 1);
    if (m.err != PROVEN_OK || !m.value.ptr) return PROVEN_ERR_NOMEM;
    proven_size_t pos = 0, len = 0;
    proven_u8str_view_t label;
    proven_err_t result = PROVEN_ERR_INVALID_FORMAT;
    for (;;) {
        proven_err_t e = proven_pem_next(text, &pos, &label, (proven_mem_mut_t){ .ptr = m.value.ptr, .size = text.size }, &len);
        if (e != PROVEN_OK) break;
        proven_mem_view_t der = { .ptr = m.value.ptr, .size = len };
        if (pk_label(label, "PRIVATE KEY")) { result = pk_pkcs8(alloc, der, rng, out); break; }
        if (pk_label(label, "EC PRIVATE KEY")) { result = pk_sec1(alloc, der, PROVEN_PKEY_NONE, out); break; }
        if (pk_label(label, "RSA PRIVATE KEY")) { result = pk_rsa(alloc, der, pk_rsa_d(der), rng, out); break; }
        if (pk_label(label, "ENCRYPTED PRIVATE KEY")) { result = PROVEN_ERR_UNSUPPORTED; break; }
    }
    pk_wipe(m.value.ptr, text.size);
    alloc.free_fn(alloc.ctx, m.value.ptr);
    return result;
}

/* ---- What a key holds ---- */

proven_err_t proven_pkey_rsa_components(const proven_pkey_t *key, proven_rsa_components_t *out) {
    if (!key || !out) return PROVEN_ERR_INVALID_ARG;
    if (key->kind != PROVEN_PKEY_RSA) return PROVEN_ERR_INVALID_STATE;
    const proven_crypto_rsa_key_t *r = &key->rsa;
    *out = (proven_rsa_components_t){
        .n = { r->n, r->n_len }, .e = { r->e, r->e_len }, .d = { key->d, key->d_len },
        .p = { r->p, r->p_len }, .q = { r->q, r->q_len },
        .dp = { r->dp, r->p_len }, .dq = { r->dq, r->q_len }, .qinv = { r->qinv, r->p_len },
    };
    return PROVEN_OK;
}

proven_err_t proven_pkey_ec_scalar(const proven_pkey_t *key, proven_mem_view_t *out) {
    if (!key || !out) return PROVEN_ERR_INVALID_ARG;
    if (key->ec_size == 0) return PROVEN_ERR_INVALID_STATE;
    *out = (proven_mem_view_t){ .ptr = key->ec_priv, .size = key->ec_size };
    return PROVEN_OK;
}

proven_err_t proven_pkey_ec_point(const proven_pkey_t *key, proven_mem_view_t *out) {
    if (!key || !out) return PROVEN_ERR_INVALID_ARG;
    if (key->ec_size == 0) return PROVEN_ERR_INVALID_STATE;
    *out = (proven_mem_view_t){ .ptr = key->ec_pub, .size = 1 + 2 * key->ec_size };
    return PROVEN_OK;
}

proven_err_t proven_pkey_spki(const proven_pkey_t *key, proven_mem_mut_t out, proven_size_t *len) {
    if (len) *len = 0;
    if (!key || !len || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_der_writer_t w;
    proven_der_writer_init(&w, out);
    proven_der_begin(&w, PROVEN_DER_SEQUENCE);
    proven_der_begin(&w, PROVEN_DER_SEQUENCE);
    if (key->kind == PROVEN_PKEY_RSA) {
        proven_der_put(&w, PROVEN_DER_OID, PK_VIEW(OID_RSA));
        proven_der_put(&w, PROVEN_DER_NULL, (proven_mem_view_t){ 0 });
        proven_der_end(&w);
        proven_der_begin(&w, PROVEN_DER_BIT_STRING);
        proven_der_begin(&w, PROVEN_DER_SEQUENCE);
        proven_der_put_uint(&w, (proven_mem_view_t){ .ptr = key->rsa.n, .size = key->rsa.n_len });
        proven_der_put_uint(&w, (proven_mem_view_t){ .ptr = key->rsa.e, .size = key->rsa.e_len });
        proven_der_end(&w);
        proven_der_end(&w);
    } else {
        proven_der_put(&w, PROVEN_DER_OID, PK_VIEW(OID_EC));
        proven_der_put(&w, PROVEN_DER_OID, key->kind == PROVEN_PKEY_P256 ? PK_VIEW(OID_P256) : PK_VIEW(OID_P384));
        proven_der_end(&w);
        proven_der_put_bits(&w, (proven_mem_view_t){ .ptr = key->ec_pub, .size = 1 + 2 * key->ec_size });
    }
    proven_der_end(&w);
    return proven_der_writer_finish(&w, len);
}

/* ---- Signing ---- */

proven_err_t proven_pkey_sign(proven_pkey_t *key, proven_sign_scheme_t scheme, proven_hmac_hash_t hash, proven_mem_view_t digest,
                              proven_rng_t rng, proven_mem_mut_t out, proven_size_t *len) {
    if (len) *len = 0;
    const bool raw = scheme == PROVEN_SIGN_RSA_PKCS1_RAW;
    const proven_size_t hlen = raw ? digest.size : proven_hmac_size(hash);
    if (!key || !len || hlen == 0 || !digest.ptr || digest.size != hlen || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (key->kind == PROVEN_PKEY_RSA) {
        if (scheme != PROVEN_SIGN_RSA_PKCS1 && scheme != PROVEN_SIGN_RSA_PSS && !raw) return PROVEN_ERR_INVALID_ARG;
        if (scheme == PROVEN_SIGN_RSA_PSS && (!proven_rng_is_valid(rng) || hash == PROVEN_HMAC_SHA1)) return PROVEN_ERR_INVALID_ARG;
        proven_crypto_rsa_key_t *r = &key->rsa;
        if (raw && digest.size + 11 > r->n_len) return PROVEN_ERR_INVALID_ARG;
        if (out.size < r->n_len) return PROVEN_ERR_OUT_OF_BOUNDS;
        /* This signature takes the pair as it stands and leaves the next one behind: squared, or
         * made afresh every PK_RSA_RENEW signatures when there is a generator to make it from.
         * The flag is held for that alone; the signature is made outside it, on the copy. */
        while (atomic_flag_test_and_set_explicit(&key->lock, memory_order_acquire)) { }
        proven_crypto_rsa_blind_t mine = key->blind;
        bool renewed = false;
        if (++key->uses >= PK_RSA_RENEW && proven_rng_is_valid(rng)) {
            proven_byte_t seed[PROVEN_CRYPTO_RSA_SIGN_MAX_BYTES];
            proven_rng_fill(rng, seed, r->n_len);
            renewed = proven_crypto_rsa_blind_make(r, seed, &key->blind);
            pk_wipe(seed, sizeof seed);
            if (renewed) key->uses = 0;
        }
        if (!renewed) proven_crypto_rsa_blind_next(r, &key->blind);
        atomic_flag_clear_explicit(&key->lock, memory_order_release);
        bool ok;
        if (raw) {
            ok = proven_crypto_rsa_sign_pkcs1_raw(r, &mine, digest, out.ptr);
        } else if (scheme == PROVEN_SIGN_RSA_PKCS1) {
            ok = proven_crypto_rsa_sign_pkcs1(r, &mine, hash, digest, out.ptr);
        } else {
            proven_byte_t salt[PROVEN_HMAC_MAX_SIZE];
            proven_rng_fill(rng, salt, hlen);
            ok = proven_crypto_rsa_sign_pss(r, &mine, hash, digest, (proven_mem_view_t){ .ptr = salt, .size = hlen }, out.ptr);
        }
        pk_wipe(&mine, sizeof mine);
        if (!ok) return PROVEN_ERR_INVALID_STATE;
        *len = r->n_len;
        return PROVEN_OK;
    }
    proven_crypto_ec_curve_t curve;
    proven_size_t size;
    if (!pk_ec_curve(key->kind, &curve, &size)) return PROVEN_ERR_INVALID_ARG;
    if (scheme != PROVEN_SIGN_ECDSA && scheme != PROVEN_SIGN_ECDSA_RAW) return PROVEN_ERR_INVALID_ARG;
    proven_byte_t rs[2 * PROVEN_CRYPTO_EC_MAX_BYTES];
    if (!proven_crypto_ecdsa_sign(curve, (int)hash, key->ec_priv, digest, rs)) return PROVEN_ERR_INVALID_STATE;
    if (scheme == PROVEN_SIGN_ECDSA_RAW) {
        if (out.size < 2 * size) return PROVEN_ERR_OUT_OF_BOUNDS;
        for (proven_size_t i = 0; i < 2 * size; ++i) out.ptr[i] = rs[i];
        *len = 2 * size;
        return PROVEN_OK;
    }
    proven_der_writer_t w;
    proven_der_writer_init(&w, out);
    proven_der_begin(&w, PROVEN_DER_SEQUENCE);
    proven_der_put_uint(&w, (proven_mem_view_t){ .ptr = rs, .size = size });
    proven_der_put_uint(&w, (proven_mem_view_t){ .ptr = rs + size, .size = size });
    proven_der_end(&w);
    return proven_der_writer_finish(&w, len);
}

/* ---- Verifying ---- */

proven_err_t proven_verify_rsa(proven_mem_view_t n, proven_mem_view_t e, proven_sign_scheme_t scheme, proven_hmac_hash_t hash,
                               proven_size_t salt_len, proven_mem_view_t digest, proven_mem_view_t sig) {
    const bool raw = scheme == PROVEN_SIGN_RSA_PKCS1_RAW;
    const proven_size_t hlen = raw ? digest.size : proven_hmac_size(hash);
    if (!n.ptr || !e.ptr || n.size == 0 || e.size == 0 || !sig.ptr || hlen == 0 || !digest.ptr || digest.size != hlen) return PROVEN_ERR_INVALID_ARG;
    if (scheme != PROVEN_SIGN_RSA_PKCS1 && scheme != PROVEN_SIGN_RSA_PSS && !raw) return PROVEN_ERR_INVALID_ARG;
    while (n.size > 0 && n.ptr[0] == 0) { n.ptr++; n.size--; }
    while (e.size > 1 && e.ptr[0] == 0) { e.ptr++; e.size--; }
    if (n.size * 8 < proven_crypto_rsa_min_bits() || n.size > 1024) return PROVEN_ERR_UNSUPPORTED;
    const bool ok = raw ? proven_crypto_rsa_verify_pkcs1_raw(n, e, digest, sig)
                  : scheme == PROVEN_SIGN_RSA_PKCS1 ? proven_crypto_rsa_verify_pkcs1(n, e, (int)hash, digest, sig)
                                                    : proven_crypto_rsa_verify_pss(n, e, (int)hash, salt_len, digest, sig);
    return ok ? PROVEN_OK : PROVEN_ERR_UNTRUSTED;
}

proven_err_t proven_verify_ecdsa(proven_pkey_kind_t curve, proven_mem_view_t point, proven_sign_scheme_t scheme,
                                 proven_mem_view_t digest, proven_mem_view_t sig) {
    proven_crypto_ec_curve_t c;
    proven_size_t size;
    if (!pk_ec_curve(curve, &c, &size) || !point.ptr || !digest.ptr || digest.size == 0 || !sig.ptr) return PROVEN_ERR_INVALID_ARG;
    bool ok;
    if (scheme == PROVEN_SIGN_ECDSA_RAW) {
        if (sig.size != 2 * size) return PROVEN_ERR_UNTRUSTED;
        ok = proven_crypto_ecdsa_verify(c, point, digest, (proven_mem_view_t){ .ptr = sig.ptr, .size = size },
                                        (proven_mem_view_t){ .ptr = sig.ptr + size, .size = size });
    } else if (scheme == PROVEN_SIGN_ECDSA) {
        ok = proven_crypto_ecdsa_verify_der(c, point, digest, sig);
    } else {
        return PROVEN_ERR_INVALID_ARG;
    }
    return ok ? PROVEN_OK : PROVEN_ERR_UNTRUSTED;
}

proven_err_t proven_verify_spki(proven_mem_view_t spki, proven_sign_scheme_t scheme, proven_hmac_hash_t hash,
                                proven_mem_view_t digest, proven_mem_view_t sig) {
    if (!spki.ptr) return PROVEN_ERR_INVALID_ARG;
    proven_mem_view_t in = spki, seq, alg, oid, bits;
    proven_u8 unused = 0;
    if (proven_der_expect(&in, PROVEN_DER_SEQUENCE, &seq) != PROVEN_OK || in.size != 0 ||
        proven_der_expect(&seq, PROVEN_DER_SEQUENCE, &alg) != PROVEN_OK || proven_der_expect(&alg, PROVEN_DER_OID, &oid) != PROVEN_OK ||
        proven_der_read_bits(&seq, &bits, &unused) != PROVEN_OK || unused != 0 || seq.size != 0) return PROVEN_ERR_INVALID_FORMAT;
    if (proven_der_oid_is(oid, PK_VIEW(OID_RSA))) {
        proven_mem_view_t rsa, n, e;
        if (proven_der_expect(&bits, PROVEN_DER_SEQUENCE, &rsa) != PROVEN_OK || bits.size != 0 ||
            proven_der_read_uint(&rsa, &n) != PROVEN_OK || proven_der_read_uint(&rsa, &e) != PROVEN_OK || rsa.size != 0) return PROVEN_ERR_INVALID_FORMAT;
        return proven_verify_rsa(n, e, scheme, hash, proven_hmac_size(hash), digest, sig);
    }
    if (proven_der_oid_is(oid, PK_VIEW(OID_EC))) {
        proven_mem_view_t curve;
        if (proven_der_expect(&alg, PROVEN_DER_OID, &curve) != PROVEN_OK || alg.size != 0) return PROVEN_ERR_INVALID_FORMAT;
        const proven_pkey_kind_t kind = pk_curve_of_oid(curve);
        if (kind == PROVEN_PKEY_NONE) return PROVEN_ERR_UNSUPPORTED;
        if (proven_hmac_size(hash) == 0 || digest.size != proven_hmac_size(hash)) return PROVEN_ERR_INVALID_ARG;
        return proven_verify_ecdsa(kind, bits, scheme, digest, sig);
    }
    return PROVEN_ERR_UNSUPPORTED;
}

/* ---- Keys held by the caller ---- */

void proven_ed25519_public(proven_byte_t pub[32], const proven_byte_t seed[32]) {
    proven_crypto_ed25519_public(pub, seed);
}

void proven_ed25519_sign(proven_byte_t sig[64], const proven_byte_t seed[32], const proven_byte_t pub[32], proven_mem_view_t msg) {
    proven_crypto_ed25519_sign(sig, seed, pub, msg);
}

proven_err_t proven_ed25519_verify(const proven_byte_t pub[32], proven_mem_view_t msg, const proven_byte_t sig[64]) {
    if (!pub || !sig || (msg.size > 0 && !msg.ptr)) return PROVEN_ERR_INVALID_ARG;
    return proven_crypto_ed25519_verify(pub, msg, sig) ? PROVEN_OK : PROVEN_ERR_UNTRUSTED;
}

proven_err_t proven_ecdsa_sign_scalar(proven_pkey_kind_t kind, proven_hmac_hash_t nonce_hash, proven_mem_view_t scalar,
                                      proven_mem_view_t digest, proven_byte_t *rs) {
    proven_crypto_ec_curve_t curve;
    proven_size_t size;
    if (!pk_ec_curve(kind, &curve, &size) || proven_hmac_size(nonce_hash) == 0 || !scalar.ptr || scalar.size != size ||
        !digest.ptr || digest.size == 0 || !rs) return PROVEN_ERR_INVALID_ARG;
    return proven_crypto_ecdsa_sign(curve, (int)nonce_hash, scalar.ptr, digest, rs) ? PROVEN_OK : PROVEN_ERR_INVALID_FORMAT;
}

proven_err_t proven_rsa_encrypt_pkcs1(proven_mem_view_t n, proven_mem_view_t e, proven_mem_view_t msg, proven_mem_view_t random,
                                      proven_mem_mut_t out, proven_size_t *len) {
    if (len) *len = 0;
    if (!n.ptr || !e.ptr || e.size == 0 || !random.ptr || !out.ptr || !len || (msg.size > 0 && !msg.ptr)) return PROVEN_ERR_INVALID_ARG;
    while (n.size > 0 && n.ptr[0] == 0) { n.ptr++; n.size--; }
    if (n.size * 8 < proven_crypto_rsa_min_bits() || n.size > 1024 || msg.size + 11 > n.size || random.size < n.size) return PROVEN_ERR_INVALID_ARG;
    if (out.size < n.size) return PROVEN_ERR_OUT_OF_BOUNDS;
    return proven_crypto_rsa_encrypt_pkcs1(n, e, msg, random.ptr, out.ptr, len) ? PROVEN_OK : PROVEN_ERR_INVALID_ARG;
}

/* For tests: how often an RSA key's blinding pair has been used, put where a test says - the
 * renewal is 65,536 signatures away otherwise. Returns the count as it was; 0 for another key. */
proven_u32 proven_pkey_test_rsa_uses_(proven_pkey_t *key, proven_u32 uses);
proven_u32 proven_pkey_test_rsa_uses_(proven_pkey_t *key, proven_u32 uses) {
    if (!key || key->kind != PROVEN_PKEY_RSA) return 0;
    while (atomic_flag_test_and_set_explicit(&key->lock, memory_order_acquire)) { }
    const proven_u32 was = key->uses;
    key->uses = uses;
    atomic_flag_clear_explicit(&key->lock, memory_order_release);
    return was;
}
