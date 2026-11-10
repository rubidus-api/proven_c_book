#include "proven_internal_cert_issue.h"
#include "proven/hash.h"
#include "proven/hmac.h"
#include "proven/encode.h"
#ifndef PROVEN_FREESTANDING
#include "proven/random.h"
#endif

/* Writing certificates: DER out, the mirror of cert.c's reader. Enough of X.509 for a
 * self-signed identity and for the chains the tests need - not a certificate authority. */

typedef struct { proven_byte_t *p; proven_size_t len, cap; bool ok; } proven_dw_t;

static void dw_bytes(proven_dw_t *w, const proven_byte_t *b, proven_size_t n) {
    if (!w->ok || n > w->cap - w->len) { w->ok = false; return; }
    for (proven_size_t i = 0; i < n; ++i) w->p[w->len + i] = b[i];
    w->len += n;
}
static void dw_byte(proven_dw_t *w, proven_byte_t b) { dw_bytes(w, &b, 1); }

/* Open an element: the tag, and room for the longest length used here. dw_close writes the
 * real length and closes the gap. */
static proven_size_t dw_open(proven_dw_t *w, proven_byte_t tag) {
    static const proven_byte_t room[3] = { 0, 0, 0 };
    dw_byte(w, tag);
    proven_size_t at = w->len;
    dw_bytes(w, room, 3);
    return at;
}

static void dw_close(proven_dw_t *w, proven_size_t at) {
    if (!w->ok) return;
    proven_size_t n = w->len - at - 3;
    proven_size_t need = n < 128 ? 1 : n < 256 ? 2 : 3;
    if (n > 0xffff) { w->ok = false; return; }
    for (proven_size_t i = 0; i < n; ++i) w->p[at + need + i] = w->p[at + 3 + i];
    if (need == 1) w->p[at] = (proven_byte_t)n;
    else if (need == 2) { w->p[at] = 0x81; w->p[at + 1] = (proven_byte_t)n; }
    else { w->p[at] = 0x82; w->p[at + 1] = (proven_byte_t)(n >> 8); w->p[at + 2] = (proven_byte_t)n; }
    w->len -= 3 - need;
}

static void dw_tlv(proven_dw_t *w, proven_byte_t tag, const proven_byte_t *b, proven_size_t n) {
    proven_size_t at = dw_open(w, tag);
    dw_bytes(w, b, n);
    dw_close(w, at);
}

#define DW_LIT(w, tag, lit) dw_tlv((w), (tag), (const proven_byte_t *)(lit), sizeof(lit) - 1)

/* A non-negative INTEGER from big-endian bytes: no needless zeros, and a zero byte in front
 * when the top bit would otherwise read as a sign. */
static void dw_uint(proven_dw_t *w, const proven_byte_t *b, proven_size_t n) {
    while (n > 1 && b[0] == 0) { b++; n--; }
    proven_size_t at = dw_open(w, 0x02);
    if (b[0] & 0x80) dw_byte(w, 0);
    dw_bytes(w, b, n);
    dw_close(w, at);
}

static void dw_two(proven_dw_t *w, int v) { dw_byte(w, (proven_byte_t)('0' + v / 10)); dw_byte(w, (proven_byte_t)('0' + v % 10)); }

/* UTCTime through 2049, GeneralizedTime from 2050, as RFC 5280 requires. */
static void dw_time(proven_dw_t *w, proven_i64 t) {
    proven_i64 days = t / 86400, rem = t % 86400;
    if (rem < 0) { rem += 86400; days--; }
    /* Civil date from a day count. */
    proven_i64 z = days + 719468;
    proven_i64 era = (z >= 0 ? z : z - 146096) / 146097;
    proven_i64 doe = z - era * 146097;
    proven_i64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    proven_i64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    proven_i64 mp = (5 * doy + 2) / 153;
    int day = (int)(doy - (153 * mp + 2) / 5 + 1);
    int month = (int)(mp < 10 ? mp + 3 : mp - 9);
    int year = (int)(yoe + era * 400 + (month <= 2 ? 1 : 0));
    bool general = year >= 2050 || year < 1950;
    proven_size_t at = dw_open(w, general ? 0x18 : 0x17);
    if (general) dw_two(w, year / 100);
    dw_two(w, year % 100); dw_two(w, month); dw_two(w, day);
    dw_two(w, (int)(rem / 3600)); dw_two(w, (int)(rem / 60 % 60)); dw_two(w, (int)(rem % 60));
    dw_byte(w, 'Z');
    dw_close(w, at);
}

static proven_size_t cstr_len(const char *s) { proven_size_t n = 0; while (s[n]) ++n; return n; }

/* Name ::= one RDN with one commonName. */
static void dw_name(proven_dw_t *w, const char *cn) {
    proven_size_t name = dw_open(w, 0x30), set = dw_open(w, 0x31), atv = dw_open(w, 0x30);
    DW_LIT(w, 0x06, "\x55\x04\x03");
    dw_tlv(w, 0x0c, (const proven_byte_t *)cn, cstr_len(cn));
    dw_close(w, atv); dw_close(w, set); dw_close(w, name);
}

#define OID_ED25519 "\x2b\x65\x70"
#define OID_EC_KEY "\x2a\x86\x48\xce\x3d\x02\x01"
#define OID_P256 "\x2a\x86\x48\xce\x3d\x03\x01\x07"
#define OID_ECDSA_SHA256 "\x2a\x86\x48\xce\x3d\x04\x03\x02"
#define OID_RSA "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01"
#define OID_RSA_SHA256 "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b"

static void dw_sig_alg(proven_dw_t *w, proven_issue_key_t issuer) {
    proven_size_t at = dw_open(w, 0x30);
    if (issuer == PROVEN_ISSUE_KEY_ED25519) DW_LIT(w, 0x06, OID_ED25519);
    else if (issuer == PROVEN_ISSUE_KEY_RSA) { DW_LIT(w, 0x06, OID_RSA_SHA256); dw_tlv(w, 0x05, (const proven_byte_t *)"", 0); }
    else DW_LIT(w, 0x06, OID_ECDSA_SHA256);
    dw_close(w, at);
}

/* SubjectPublicKeyInfo for an RSA key: rsaEncryption with NULL parameters, and
 * RSAPublicKey ::= SEQUENCE { modulus, publicExponent } inside the bit string. */
static void dw_spki_rsa(proven_dw_t *w, const proven_crypto_rsa_key_t *k) {
    proven_size_t spki = dw_open(w, 0x30), alg = dw_open(w, 0x30);
    DW_LIT(w, 0x06, OID_RSA);
    dw_tlv(w, 0x05, (const proven_byte_t *)"", 0);
    dw_close(w, alg);
    proven_size_t bits = dw_open(w, 0x03);
    dw_byte(w, 0);
    proven_size_t seq = dw_open(w, 0x30);
    dw_uint(w, k->n, k->n_len);
    dw_uint(w, k->e, k->e_len);
    dw_close(w, seq); dw_close(w, bits); dw_close(w, spki);
}

static bool dw_spki(proven_dw_t *w, proven_issue_key_t kind, const proven_byte_t key[32]) {
    proven_byte_t pub[66];
    proven_size_t pub_len;
    pub[0] = 0;                                   /* no unused bits */
    proven_size_t spki = dw_open(w, 0x30), alg = dw_open(w, 0x30);
    if (kind == PROVEN_ISSUE_KEY_ED25519) {
        DW_LIT(w, 0x06, OID_ED25519);
        proven_crypto_ed25519_public(pub + 1, key);
        pub_len = 33;
    } else {
        DW_LIT(w, 0x06, OID_EC_KEY);
        DW_LIT(w, 0x06, OID_P256);
        if (!proven_crypto_ec_public(PROVEN_CRYPTO_EC_P256, key, pub + 1)) return false;
        pub_len = 66;
    }
    dw_close(w, alg);
    dw_tlv(w, 0x03, pub, pub_len);
    dw_close(w, spki);
    return true;
}

/* One extension: the identifier, the critical flag when set, and the value's wrapper opened. */
static proven_size_t dw_ext_open(proven_dw_t *w, const char *oid, proven_size_t oid_len, bool critical, proven_size_t *ext) {
    static const proven_byte_t yes = 0xff;
    *ext = dw_open(w, 0x30);
    dw_tlv(w, 0x06, (const proven_byte_t *)oid, oid_len);
    if (critical) dw_tlv(w, 0x01, &yes, 1);
    return dw_open(w, 0x04);
}

proven_err_t proven_tls_issue_(const proven_tls_issue_t *q, proven_mem_mut_t out, proven_size_t *len) {
    if (!q || !out.ptr || !len || !q->subject || !q->issuer) return PROVEN_ERR_INVALID_ARG;
    if (q->subject_kind == PROVEN_ISSUE_KEY_RSA ? !q->subject_rsa : !q->subject_key) return PROVEN_ERR_INVALID_ARG;
    if (q->issuer_kind == PROVEN_ISSUE_KEY_RSA ? !q->issuer_rsa : !q->issuer_key) return PROVEN_ERR_INVALID_ARG;
    if (q->not_after <= q->not_before) return PROVEN_ERR_INVALID_ARG;
    proven_byte_t tbs_buf[1400];
    proven_dw_t t = { tbs_buf, 0, sizeof tbs_buf, true };
    static const proven_byte_t v3 = 2, yes = 0xff;
    proven_size_t tbs = dw_open(&t, 0x30);
    proven_size_t ver = dw_open(&t, 0xa0);
    dw_tlv(&t, 0x02, &v3, 1);
    dw_close(&t, ver);
    proven_byte_t serial[16];
    for (int i = 0; i < 16; ++i) serial[i] = q->serial[i];
    serial[0] = (proven_byte_t)((serial[0] & 0x7f) | 0x01);     /* positive and never zero */
    dw_uint(&t, serial, 16);
    dw_sig_alg(&t, q->issuer_kind);
    dw_name(&t, q->issuer);
    proven_size_t validity = dw_open(&t, 0x30);
    dw_time(&t, q->not_before); dw_time(&t, q->not_after);
    dw_close(&t, validity);
    dw_name(&t, q->subject);
    if (q->subject_kind == PROVEN_ISSUE_KEY_RSA) { if (!q->subject_rsa) return PROVEN_ERR_INVALID_ARG; dw_spki_rsa(&t, q->subject_rsa); }
    else if (!dw_spki(&t, q->subject_kind, q->subject_key)) return PROVEN_ERR_INVALID_ARG;

    proven_size_t exts_wrap = dw_open(&t, 0xa3), exts = dw_open(&t, 0x30), ext, val, seq;
    val = dw_ext_open(&t, "\x55\x1d\x13", 3, true, &ext);         /* basicConstraints */
    seq = dw_open(&t, 0x30);
    if (q->is_ca) dw_tlv(&t, 0x01, &yes, 1);
    dw_close(&t, seq); dw_close(&t, val); dw_close(&t, ext);
    val = dw_ext_open(&t, "\x55\x1d\x0f", 3, true, &ext);         /* keyUsage */
    if (q->is_ca) DW_LIT(&t, 0x03, "\x01\x06");                   /* keyCertSign, cRLSign */
    else DW_LIT(&t, 0x03, "\x07\x80");                            /* digitalSignature */
    dw_close(&t, val); dw_close(&t, ext);
    if (q->name_count > 0) {
        val = dw_ext_open(&t, "\x55\x1d\x11", 3, false, &ext);    /* subjectAltName */
        seq = dw_open(&t, 0x30);
        for (proven_size_t i = 0; i < q->name_count; ++i) {
            proven_byte_t ip[16];
            if (!q->names[i].ptr || q->names[i].size == 0 || q->names[i].size > 253) return PROVEN_ERR_INVALID_ARG;
            proven_size_t ip_len = proven_cert_host_as_ip_(q->names[i], ip);
            if (ip_len > 0) dw_tlv(&t, 0x87, ip, ip_len);
            else dw_tlv(&t, 0x82, q->names[i].ptr, q->names[i].size);
        }
        dw_close(&t, seq); dw_close(&t, val); dw_close(&t, ext);
    }
    if (q->eku != 0) {
        val = dw_ext_open(&t, "\x55\x1d\x25", 3, q->eku_critical, &ext);    /* extendedKeyUsage */
        seq = dw_open(&t, 0x30);
        if (q->eku & PROVEN_CERT_EKU_SERVER_AUTH) DW_LIT(&t, 0x06, "\x2b\x06\x01\x05\x05\x07\x03\x01");
        if (q->eku & PROVEN_CERT_EKU_CLIENT_AUTH) DW_LIT(&t, 0x06, "\x2b\x06\x01\x05\x05\x07\x03\x02");
        if (q->eku & PROVEN_CERT_EKU_CODE_SIGNING) DW_LIT(&t, 0x06, "\x2b\x06\x01\x05\x05\x07\x03\x03");
        if (q->eku & PROVEN_CERT_EKU_TIME_STAMPING) DW_LIT(&t, 0x06, "\x2b\x06\x01\x05\x05\x07\x03\x08");
        dw_close(&t, seq); dw_close(&t, val); dw_close(&t, ext);
    }
    if (q->ski.size > 0) {
        val = dw_ext_open(&t, "\x55\x1d\x0e", 3, false, &ext);    /* subjectKeyIdentifier */
        dw_tlv(&t, 0x04, q->ski.ptr, q->ski.size);
        dw_close(&t, val); dw_close(&t, ext);
    }
    if (q->aki.size > 0) {
        val = dw_ext_open(&t, "\x55\x1d\x23", 3, false, &ext);    /* authorityKeyIdentifier */
        seq = dw_open(&t, 0x30);
        dw_tlv(&t, 0x80, q->aki.ptr, q->aki.size);
        dw_close(&t, seq); dw_close(&t, val); dw_close(&t, ext);
    }
    dw_close(&t, exts); dw_close(&t, exts_wrap);
    dw_close(&t, tbs);
    if (!t.ok) return PROVEN_ERR_OUT_OF_BOUNDS;

    proven_byte_t sig[PROVEN_CRYPTO_RSA_SIGN_MAX_BYTES];
    proven_size_t sig_len;
    proven_mem_view_t signed_part = { .ptr = tbs_buf, .size = t.len };
    if (q->issuer_kind == PROVEN_ISSUE_KEY_RSA) {
        proven_byte_t digest[32];
        proven_sha256(signed_part, digest);
        if (!q->issuer_rsa || !proven_crypto_rsa_sign_pkcs1(q->issuer_rsa, (void *)0, PROVEN_HMAC_SHA256, (proven_mem_view_t){ .ptr = digest, .size = 32 }, sig)) return PROVEN_ERR_INVALID_ARG;
        sig_len = q->issuer_rsa->n_len;
    } else if (q->issuer_kind == PROVEN_ISSUE_KEY_ED25519) {
        proven_byte_t pub[32];
        proven_crypto_ed25519_public(pub, q->issuer_key);
        proven_crypto_ed25519_sign(sig, q->issuer_key, pub, signed_part);
        sig_len = 64;
    } else {
        proven_byte_t digest[32], raw[64];
        proven_sha256(signed_part, digest);
        if (!proven_crypto_ecdsa_sign(PROVEN_CRYPTO_EC_P256, PROVEN_HMAC_SHA256, q->issuer_key, (proven_mem_view_t){ .ptr = digest, .size = 32 }, raw)) return PROVEN_ERR_INVALID_ARG;
        proven_dw_t s = { sig, 0, 80, true };
        proven_size_t at = dw_open(&s, 0x30);
        dw_uint(&s, raw, 32); dw_uint(&s, raw + 32, 32);
        dw_close(&s, at);
        sig_len = s.len;
    }
    proven_dw_t c = { out.ptr, 0, out.size, true };
    proven_size_t cert = dw_open(&c, 0x30);
    dw_bytes(&c, tbs_buf, t.len);
    dw_sig_alg(&c, q->issuer_kind);
    proven_size_t bits = dw_open(&c, 0x03);
    dw_byte(&c, 0);
    dw_bytes(&c, sig, sig_len);
    dw_close(&c, bits);
    dw_close(&c, cert);
    if (!c.ok) return PROVEN_ERR_OUT_OF_BOUNDS;
    *len = c.len;
    return PROVEN_OK;
}

proven_size_t proven_tls_rsa_key_der_(const proven_crypto_rsa_key_t *k, const proven_byte_t *d, bool pkcs8, proven_mem_mut_t out) {
    static const proven_byte_t zero = 0;
    proven_dw_t w = { out.ptr, 0, out.size, true };
    proven_size_t info = 0, octets = 0;
    if (pkcs8) {
        info = dw_open(&w, 0x30);
        dw_uint(&w, &zero, 1);
        proven_size_t alg = dw_open(&w, 0x30);
        DW_LIT(&w, 0x06, OID_RSA);
        dw_tlv(&w, 0x05, (const proven_byte_t *)"", 0);
        dw_close(&w, alg);
        octets = dw_open(&w, 0x04);
    }
    proven_size_t seq = dw_open(&w, 0x30);
    dw_uint(&w, &zero, 1);
    dw_uint(&w, k->n, k->n_len); dw_uint(&w, k->e, k->e_len); dw_uint(&w, d, k->n_len);
    dw_uint(&w, k->p, k->p_len); dw_uint(&w, k->q, k->q_len);
    dw_uint(&w, k->dp, k->p_len); dw_uint(&w, k->dq, k->q_len); dw_uint(&w, k->qinv, k->p_len);
    dw_close(&w, seq);
    if (pkcs8) { dw_close(&w, octets); dw_close(&w, info); }
    return w.ok ? w.len : 0;
}

proven_size_t proven_tls_key_der_(proven_issue_key_t kind, const proven_byte_t key[32], bool sec1, proven_byte_t out[80]) {
    static const proven_byte_t zero = 0, one = 1;
    proven_dw_t w = { out, 0, 80, true };
    if (kind == PROVEN_ISSUE_KEY_P256 && sec1) {
        /* ECPrivateKey ::= SEQUENCE { 1, privateKey, [0] curve } */
        proven_size_t seq = dw_open(&w, 0x30);
        dw_tlv(&w, 0x02, &one, 1);
        dw_tlv(&w, 0x04, key, 32);
        proven_size_t params = dw_open(&w, 0xa0);
        DW_LIT(&w, 0x06, OID_P256);
        dw_close(&w, params); dw_close(&w, seq);
        return w.ok ? w.len : 0;
    }
    /* PrivateKeyInfo ::= SEQUENCE { 0, algorithm, OCTET STRING { the key in its own form } } */
    proven_size_t seq = dw_open(&w, 0x30);
    dw_tlv(&w, 0x02, &zero, 1);
    proven_size_t alg = dw_open(&w, 0x30);
    if (kind == PROVEN_ISSUE_KEY_ED25519) DW_LIT(&w, 0x06, OID_ED25519);
    else { DW_LIT(&w, 0x06, OID_EC_KEY); DW_LIT(&w, 0x06, OID_P256); }
    dw_close(&w, alg);
    proven_size_t wrap = dw_open(&w, 0x04);
    if (kind == PROVEN_ISSUE_KEY_ED25519) {
        dw_tlv(&w, 0x04, key, 32);
    } else {
        proven_size_t inner = dw_open(&w, 0x30);
        dw_tlv(&w, 0x02, &one, 1);
        dw_tlv(&w, 0x04, key, 32);
        dw_close(&w, inner);
    }
    dw_close(&w, wrap); dw_close(&w, seq);
    return w.ok ? w.len : 0;
}

proven_err_t proven_tls_pem_write_(const char *label, proven_mem_view_t der, proven_mem_mut_t out, proven_size_t *len) {
    static const char dashes[] = "-----", begin[] = "BEGIN ", end[] = "END ";
    proven_size_t label_len = cstr_len(label), b64 = proven_base64_encoded_size(der.size);
    proven_size_t lines = (b64 + 63) / 64;
    proven_size_t need = (5 + 6 + label_len + 5 + 1) + b64 + lines + (5 + 4 + label_len + 5 + 1);
    if (!out.ptr || need > out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_size_t n = 0, written = 0;
    proven_byte_t *o = out.ptr;
#define PUT(s, k) do { for (proven_size_t i_ = 0; i_ < (k); ++i_) o[n++] = (proven_byte_t)(s)[i_]; } while (0)
    PUT(dashes, 5); PUT(begin, 6); PUT(label, label_len); PUT(dashes, 5); o[n++] = '\n';
    /* Encode at the end of the buffer, then lay it out in lines from the front: they do not
     * overlap until the last line, which is copied forwards. */
    proven_byte_t *tmp = out.ptr + out.size - b64;
    proven_err_t e = proven_base64_encode(der, tmp, b64, &written);
    if (e != PROVEN_OK) return e;
    for (proven_size_t i = 0; i < b64; ++i) {
        o[n++] = tmp[i];
        if (i % 64 == 63 || i + 1 == b64) o[n++] = '\n';
    }
    PUT(dashes, 5); PUT(end, 4); PUT(label, label_len); PUT(dashes, 5); o[n++] = '\n';
#undef PUT
    *len = n;
    return PROVEN_OK;
}

#ifndef PROVEN_FREESTANDING
static void issue_os_random(void *ctx, proven_byte_t *out, proven_size_t len) { (void)ctx; (void)proven_random_bytes(out, len); }
#endif

proven_err_t proven_tls_self_signed_(const proven_u8str_view_t *names, proven_size_t name_count,
                                     proven_i64 not_before, proven_i64 not_after,
                                     proven_issue_random_fn random, void *random_ctx,
                                     proven_mem_mut_t certificate_pem, proven_size_t *certificate_len,
                                     proven_mem_mut_t private_key_pem, proven_size_t *private_key_len) {
    if (!names || name_count == 0 || name_count > 16 || !certificate_len || !private_key_len || not_after <= not_before) return PROVEN_ERR_INVALID_ARG;
    for (proven_size_t i = 0; i < name_count; ++i) if (!names[i].ptr || names[i].size == 0 || names[i].size > 253) return PROVEN_ERR_INVALID_ARG;
#ifndef PROVEN_FREESTANDING
    if (!random) random = issue_os_random;
#else
    if (!random) return PROVEN_ERR_INVALID_ARG;
#endif
    proven_byte_t key[32], der[1400], key_der[80];
    char subject[64];
    proven_size_t cn = names[0].size < sizeof subject - 1 ? names[0].size : sizeof subject - 1;
    for (proven_size_t i = 0; i < cn; ++i) subject[i] = (char)names[0].ptr[i];
    subject[cn] = 0;
    proven_tls_issue_t q = {
        .subject = subject, .issuer = subject,
        .subject_kind = PROVEN_ISSUE_KEY_ED25519, .subject_key = key,
        .issuer_kind = PROVEN_ISSUE_KEY_ED25519, .issuer_key = key,
        .is_ca = false, .eku = PROVEN_CERT_EKU_SERVER_AUTH | PROVEN_CERT_EKU_CLIENT_AUTH,
        .names = names, .name_count = name_count, .not_before = not_before, .not_after = not_after,
    };
    random(random_ctx, key, 32);
    random(random_ctx, q.serial, 16);
    proven_size_t der_len = 0;
    proven_err_t e = proven_tls_issue_(&q, (proven_mem_mut_t){ .ptr = der, .size = sizeof der }, &der_len);
    if (e == PROVEN_OK) e = proven_tls_pem_write_("CERTIFICATE", (proven_mem_view_t){ .ptr = der, .size = der_len }, certificate_pem, certificate_len);
    if (e == PROVEN_OK) {
        proven_size_t kn = proven_tls_key_der_(PROVEN_ISSUE_KEY_ED25519, key, false, key_der);
        e = proven_tls_pem_write_("PRIVATE KEY", (proven_mem_view_t){ .ptr = key_der, .size = kn }, private_key_pem, private_key_len);
    }
    proven_mem_wipe((proven_mem_mut_t){ .ptr = key, .size = sizeof key });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = key_der, .size = sizeof key_der });
    return e;
}
