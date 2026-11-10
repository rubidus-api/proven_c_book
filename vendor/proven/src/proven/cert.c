#include "proven/cert.h"
#include "proven/hash.h"
#include "proven/hmac.h"
#include "proven_internal_crypto.h"
#include "proven_internal_der.h"

/* X.509: a strict DER reader, the certificate fields validation needs, host-name matching,
 * PEM, a store of trust anchors, and path building with the checks of RFC 5280 that this
 * library makes. Everything read here is public data from a peer: the concern is not timing,
 * it is never reading outside the buffer and never accepting an encoding with two readings. */

/* ---- Time ---- */

static bool two_digits(const proven_byte_t *p, int *out) {
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return false;
    *out = (p[0] - '0') * 10 + (p[1] - '0');
    return true;
}

/* UTCTime YYMMDDHHMMSSZ or GeneralizedTime YYYYMMDDHHMMSSZ, as RFC 5280 restricts them. */
static bool der_time(proven_der_t *in, proven_i64 *out) {
    proven_byte_t tag;
    proven_der_t b;
    int cc = 0, yy, mo, dd, hh, mi, ss;
    if (!der_read(in, &tag, &b, NULL)) return false;
    const proven_byte_t *p = b.p;
    if (tag == DER_UTCTIME) {
        if (b.n != 13) return false;
    } else if (tag == DER_GENTIME) {
        if (b.n != 15 || !two_digits(p, &cc)) return false;
        p += 2;
    } else {
        return false;
    }
    if (!two_digits(p, &yy) || !two_digits(p + 2, &mo) || !two_digits(p + 4, &dd) || !two_digits(p + 6, &hh) ||
        !two_digits(p + 8, &mi) || !two_digits(p + 10, &ss) || p[12] != 'Z') return false;
    int year = tag == DER_UTCTIME ? (yy >= 50 ? 1900 + yy : 2000 + yy) : cc * 100 + yy;
    static const int days_in[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (mo < 1 || mo > 12 || dd < 1 || dd > days_in[mo - 1] + (mo == 2 && leap ? 1 : 0) || hh > 23 || mi > 59 || ss > 59) return false;
    /* Days since 1970-01-01 (the civil-from-days algorithm, run forwards). */
    proven_i64 y = year - (mo <= 2 ? 1 : 0);
    proven_i64 era = (y >= 0 ? y : y - 399) / 400;
    proven_i64 yoe = y - era * 400;
    proven_i64 doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + dd - 1;
    proven_i64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    proven_i64 days = era * 146097 + doe - 719468;
    *out = days * 86400 + hh * 3600 + mi * 60 + ss;
    return true;
}

/* ---- Algorithm identifiers ---- */

#define OID_RSA "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01"
#define OID_RSA_SHA256 "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b"
#define OID_RSA_SHA384 "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0c"
#define OID_RSA_SHA512 "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0d"
#define OID_RSA_PSS "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0a"
#define OID_MGF1 "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x08"
#define OID_EC_KEY "\x2a\x86\x48\xce\x3d\x02\x01"
#define OID_P256 "\x2a\x86\x48\xce\x3d\x03\x01\x07"
#define OID_P384 "\x2b\x81\x04\x00\x22"
#define OID_ECDSA_SHA256 "\x2a\x86\x48\xce\x3d\x04\x03\x02"
#define OID_ECDSA_SHA384 "\x2a\x86\x48\xce\x3d\x04\x03\x03"
#define OID_ECDSA_SHA512 "\x2a\x86\x48\xce\x3d\x04\x03\x04"
#define OID_ED25519 "\x2b\x65\x70"
#define OID_SHA256 "\x60\x86\x48\x01\x65\x03\x04\x02\x01"
#define OID_SHA384 "\x60\x86\x48\x01\x65\x03\x04\x02\x02"
#define OID_SHA512 "\x60\x86\x48\x01\x65\x03\x04\x02\x03"

#define OID_EXT_KEY_USAGE "\x55\x1d\x0f"
#define OID_EXT_ALT_NAME "\x55\x1d\x11"
#define OID_EXT_BASIC "\x55\x1d\x13"
#define OID_EXT_NAME_CONSTRAINTS "\x55\x1d\x1e"
#define OID_EXT_POLICIES "\x55\x1d\x20"
#define OID_EXT_POLICY_MAPPINGS "\x55\x1d\x21"
#define OID_EXT_POLICY_CONSTRAINTS "\x55\x1d\x24"
#define OID_EXT_EKU "\x55\x1d\x25"
#define OID_EXT_INHIBIT_ANY "\x55\x1d\x36"
#define OID_EKU_SERVER "\x2b\x06\x01\x05\x05\x07\x03\x01"
#define OID_EKU_CLIENT "\x2b\x06\x01\x05\x05\x07\x03\x02"
#define OID_EKU_ANY "\x55\x1d\x25\x00"
#define OID_EKU_CODE_SIGNING "\x2b\x06\x01\x05\x05\x07\x03\x03"
#define OID_EKU_TIME_STAMPING "\x2b\x06\x01\x05\x05\x07\x03\x08"
#define OID_EXT_SKI "\x55\x1d\x0e"
#define OID_EXT_AKI "\x55\x1d\x23"

static bool hash_from_oid(const proven_der_t *oid, int *hash) {
    if (DER_IS(oid, OID_SHA256)) *hash = PROVEN_HMAC_SHA256;
    else if (DER_IS(oid, OID_SHA384)) *hash = PROVEN_HMAC_SHA384;
    else if (DER_IS(oid, OID_SHA512)) *hash = PROVEN_HMAC_SHA512;
    else return false;
    return true;
}

/* An AlgorithmIdentifier whose parameters are absent or NULL. */
static bool params_absent_or_null(proven_der_t rest) {
    if (rest.n == 0) return true;
    proven_der_t b;
    return der_expect(&rest, DER_NULL, &b, NULL) && b.n == 0 && rest.n == 0;
}

/* RSASSA-PSS-params. The defaults are SHA-1, which is not accepted, so every field that
 * matters has to be present. */
static bool parse_pss_params(proven_der_t rest, int *hash, proven_size_t *salt_len) {
    proven_der_t seq, f, alg, oid, mgf_alg, mgf_oid, mgf_hash, mgf_hash_oid;
    int mgf = -1;
    proven_u32 salt = 0, trailer = 1;
    if (!der_expect(&rest, DER_SEQUENCE, &seq, NULL) || rest.n != 0) return false;
    if (!der_expect(&seq, 0xa0, &f, NULL)) return false;
    if (!der_expect(&f, DER_SEQUENCE, &alg, NULL) || f.n != 0) return false;
    if (!der_expect(&alg, DER_OID, &oid, NULL) || !hash_from_oid(&oid, hash) || !params_absent_or_null(alg)) return false;
    if (!der_expect(&seq, 0xa1, &f, NULL)) return false;
    if (!der_expect(&f, DER_SEQUENCE, &mgf_alg, NULL) || f.n != 0) return false;
    if (!der_expect(&mgf_alg, DER_OID, &mgf_oid, NULL) || !DER_IS(&mgf_oid, OID_MGF1)) return false;
    if (!der_expect(&mgf_alg, DER_SEQUENCE, &mgf_hash, NULL) || mgf_alg.n != 0) return false;
    if (!der_expect(&mgf_hash, DER_OID, &mgf_hash_oid, NULL) || !hash_from_oid(&mgf_hash_oid, &mgf) || !params_absent_or_null(mgf_hash)) return false;
    if (mgf != *hash) return false;
    if (!der_expect(&seq, 0xa2, &f, NULL) || !der_small_uint(&f, &salt) || f.n != 0) return false;
    if (seq.n != 0) {
        if (!der_expect(&seq, 0xa3, &f, NULL) || !der_small_uint(&f, &trailer) || f.n != 0 || trailer != 1 || seq.n != 0) return false;
    }
    *salt_len = salt;
    return true;
}

static void parse_sig_alg(proven_der_t alg, proven_cert_t *c) {
    proven_der_t oid;
    c->sig_kind = PROVEN_CERT_SIG_UNKNOWN;
    c->sig_hash = 0;
    c->sig_salt_len = 0;
    if (!der_expect(&alg, DER_OID, &oid, NULL)) return;
    if (DER_IS(&oid, OID_RSA_SHA256) || DER_IS(&oid, OID_RSA_SHA384) || DER_IS(&oid, OID_RSA_SHA512)) {
        if (!params_absent_or_null(alg)) return;
        c->sig_hash = DER_IS(&oid, OID_RSA_SHA256) ? PROVEN_HMAC_SHA256 : DER_IS(&oid, OID_RSA_SHA384) ? PROVEN_HMAC_SHA384 : PROVEN_HMAC_SHA512;
        c->sig_kind = PROVEN_CERT_SIG_RSA_PKCS1;
    } else if (DER_IS(&oid, OID_RSA_PSS)) {
        if (parse_pss_params(alg, &c->sig_hash, &c->sig_salt_len)) c->sig_kind = PROVEN_CERT_SIG_RSA_PSS;
    } else if (DER_IS(&oid, OID_ECDSA_SHA256) || DER_IS(&oid, OID_ECDSA_SHA384) || DER_IS(&oid, OID_ECDSA_SHA512)) {
        if (alg.n != 0) return;
        c->sig_hash = DER_IS(&oid, OID_ECDSA_SHA256) ? PROVEN_HMAC_SHA256 : DER_IS(&oid, OID_ECDSA_SHA384) ? PROVEN_HMAC_SHA384 : PROVEN_HMAC_SHA512;
        c->sig_kind = PROVEN_CERT_SIG_ECDSA;
    } else if (DER_IS(&oid, OID_ED25519)) {
        if (alg.n != 0) return;
        c->sig_kind = PROVEN_CERT_SIG_ED25519;
    }
}

/* SubjectPublicKeyInfo. A key this library cannot use leaves key_kind UNKNOWN; a malformed
 * structure is an error. */
static bool parse_spki(proven_der_t spki, proven_cert_t *c) {
    proven_der_t alg, oid, bits;
    c->key_kind = PROVEN_CERT_KEY_UNKNOWN;
    if (!der_expect(&spki, DER_SEQUENCE, &alg, NULL) || !der_expect(&alg, DER_OID, &oid, NULL)) return false;
    if (!der_expect(&spki, DER_BITSTRING, &bits, NULL) || spki.n != 0 || bits.n < 1 || bits.p[0] != 0) return false;
    bits.p++; bits.n--;
    if (DER_IS(&oid, OID_RSA) || DER_IS(&oid, OID_RSA_PSS)) {
        proven_der_t seq;
        if (DER_IS(&oid, OID_RSA) && !params_absent_or_null(alg)) return false;
        if (!der_expect(&bits, DER_SEQUENCE, &seq, NULL) || bits.n != 0) return false;
        if (!der_uint(&seq, &c->rsa_n) || !der_uint(&seq, &c->rsa_e) || seq.n != 0) return false;
        c->key_kind = PROVEN_CERT_KEY_RSA;
    } else if (DER_IS(&oid, OID_EC_KEY)) {
        proven_der_t curve;
        if (!der_expect(&alg, DER_OID, &curve, NULL) || alg.n != 0) return true;       /* explicit parameters: not used */
        if (DER_IS(&curve, OID_P256) && bits.n == 65 && bits.p[0] == 0x04) c->key_kind = PROVEN_CERT_KEY_EC_P256;
        else if (DER_IS(&curve, OID_P384) && bits.n == 97 && bits.p[0] == 0x04) c->key_kind = PROVEN_CERT_KEY_EC_P384;
        else return true;
        c->key.ptr = bits.p; c->key.size = bits.n;
    } else if (DER_IS(&oid, OID_ED25519)) {
        if (alg.n != 0 || bits.n != 32) return false;
        c->key_kind = PROVEN_CERT_KEY_ED25519;
        c->key.ptr = bits.p; c->key.size = bits.n;
    }
    return true;
}

static bool parse_extensions(proven_der_t exts, proven_cert_t *c) {
    proven_der_t list;
    unsigned seen = 0;
    if (!der_expect(&exts, DER_SEQUENCE, &list, NULL) || exts.n != 0 || list.n == 0) return false;
    while (list.n > 0) {
        proven_der_t ext, oid, value, flag;
        bool critical = false;
        if (!der_expect(&list, DER_SEQUENCE, &ext, NULL) || !der_expect(&ext, DER_OID, &oid, NULL)) return false;
        if (der_expect(&ext, DER_BOOLEAN, &flag, NULL)) {
            if (flag.n != 1 || flag.p[0] != 0xff) return false;      /* DER: TRUE is FF, and FALSE is left out */
            critical = true;
        }
        if (!der_expect(&ext, DER_OCTETS, &value, NULL) || ext.n != 0) return false;
        unsigned bit = 0;
        if (DER_IS(&oid, OID_EXT_BASIC)) {
            proven_der_t seq, b;
            bit = 1;
            if (!der_expect(&value, DER_SEQUENCE, &seq, NULL) || value.n != 0) return false;
            if (der_expect(&seq, DER_BOOLEAN, &b, NULL)) {
                if (b.n != 1 || b.p[0] != 0xff) return false;
                c->is_ca = true;
            }
            if (seq.n != 0) {
                if (!der_small_uint(&seq, &c->path_len) || seq.n != 0) return false;
                c->has_path_len = true;
            }
        } else if (DER_IS(&oid, OID_EXT_KEY_USAGE)) {
            proven_der_t bits;
            bit = 2;
            if (!der_expect(&value, DER_BITSTRING, &bits, NULL) || value.n != 0 || bits.n < 1 || bits.p[0] > 7) return false;
            c->has_key_usage = true;
            for (proven_size_t i = 0; i < 16 && i / 8 + 1 < bits.n; ++i) {
                if (bits.p[1 + i / 8] & (0x80u >> (i % 8))) c->key_usage |= (proven_u32)1 << i;
            }
        } else if (DER_IS(&oid, OID_EXT_EKU)) {
            proven_der_t seq;
            bit = 4;
            if (!der_expect(&value, DER_SEQUENCE, &seq, NULL) || value.n != 0 || seq.n == 0) return false;
            c->has_ext_key_usage = true;
            c->ext_key_usage_critical = critical;
            while (seq.n > 0) {
                proven_der_t purpose;
                if (!der_expect(&seq, DER_OID, &purpose, NULL)) return false;
                c->ext_key_usage_count++;
                if (DER_IS(&purpose, OID_EKU_SERVER)) c->ext_key_usage |= PROVEN_CERT_EKU_SERVER_AUTH;
                else if (DER_IS(&purpose, OID_EKU_CLIENT)) c->ext_key_usage |= PROVEN_CERT_EKU_CLIENT_AUTH;
                else if (DER_IS(&purpose, OID_EKU_ANY)) c->ext_key_usage |= PROVEN_CERT_EKU_ANY;
                else if (DER_IS(&purpose, OID_EKU_CODE_SIGNING)) c->ext_key_usage |= PROVEN_CERT_EKU_CODE_SIGNING;
                else if (DER_IS(&purpose, OID_EKU_TIME_STAMPING)) c->ext_key_usage |= PROVEN_CERT_EKU_TIME_STAMPING;
                else c->ext_key_usage |= PROVEN_CERT_EKU_OTHER;
            }
        } else if (DER_IS(&oid, OID_EXT_SKI)) {
            proven_der_t id;
            bit = 32;
            if (!der_expect(&value, DER_OCTETS, &id, NULL) || value.n != 0) return false;
            c->subject_key_id.ptr = id.p; c->subject_key_id.size = id.n;
        } else if (DER_IS(&oid, OID_EXT_AKI)) {
            /* AuthorityKeyIdentifier ::= SEQUENCE { [0] keyIdentifier OPTIONAL, [1] issuer OPTIONAL, [2] serial OPTIONAL } */
            proven_der_t seq, id;
            bit = 64;
            if (!der_expect(&value, DER_SEQUENCE, &seq, NULL) || value.n != 0) return false;
            if (der_expect(&seq, 0x80, &id, NULL)) { c->authority_key_id.ptr = id.p; c->authority_key_id.size = id.n; }
        } else if (DER_IS(&oid, OID_EXT_ALT_NAME)) {
            proven_der_t seq;
            bit = 8;
            if (!der_expect(&value, DER_SEQUENCE, &seq, NULL) || value.n != 0 || seq.n == 0) return false;
            c->alt_names.ptr = seq.p; c->alt_names.size = seq.n;
        } else if (DER_IS(&oid, OID_EXT_NAME_CONSTRAINTS)) {
            proven_der_t seq;
            bit = 16;
            if (!der_expect(&value, DER_SEQUENCE, &seq, NULL) || value.n != 0 || seq.n == 0) return false;
            c->name_constraints.ptr = seq.p; c->name_constraints.size = seq.n;
        } else if (DER_IS(&oid, OID_EXT_POLICIES) || DER_IS(&oid, OID_EXT_POLICY_MAPPINGS) ||
                   DER_IS(&oid, OID_EXT_POLICY_CONSTRAINTS) || DER_IS(&oid, OID_EXT_INHIBIT_ANY)) {
            /* Recognised and not enforced: no caller of this library can ask for a policy. */
        } else if (critical) {
            c->unknown_critical = true;
        }
        if (bit) {
            if (seen & bit) return false;                           /* the same extension twice */
            seen |= bit;
        }
    }
    return true;
}

proven_err_t proven_cert_parse(proven_mem_view_t der, proven_cert_t *out) {
    if (!out || !der.ptr) return PROVEN_ERR_INVALID_ARG;
    proven_cert_t c = { 0 };
    proven_der_t in = { der.ptr, der.size }, cert, tbs, f, sig_alg, tbs_alg, validity, sig;
    proven_mem_view_t sig_alg_whole, tbs_alg_whole, spki_whole;
    c.der = der;
    c.version = 1;
    if (!der_expect(&in, DER_SEQUENCE, &cert, NULL) || in.n != 0) return PROVEN_ERR_INVALID_FORMAT;
    if (!der_expect(&cert, DER_SEQUENCE, &tbs, &c.tbs)) return PROVEN_ERR_INVALID_FORMAT;
    if (!der_expect(&cert, DER_SEQUENCE, &sig_alg, &sig_alg_whole)) return PROVEN_ERR_INVALID_FORMAT;
    if (!der_expect(&cert, DER_BITSTRING, &sig, NULL) || cert.n != 0 || sig.n < 1 || sig.p[0] != 0) return PROVEN_ERR_INVALID_FORMAT;
    c.signature.ptr = sig.p + 1; c.signature.size = sig.n - 1;

    if (der_expect(&tbs, 0xa0, &f, NULL)) {
        proven_u32 v = 0;
        if (!der_small_uint(&f, &v) || f.n != 0 || v < 1 || v > 2) return PROVEN_ERR_INVALID_FORMAT;   /* an explicit v1 is not DER */
        c.version = (int)v + 1;
    }
    if (!der_expect(&tbs, DER_INTEGER, &f, NULL) || f.n == 0 || f.n > 21) return PROVEN_ERR_INVALID_FORMAT;
    c.serial.ptr = f.p; c.serial.size = f.n;
    if (!der_expect(&tbs, DER_SEQUENCE, &tbs_alg, &tbs_alg_whole)) return PROVEN_ERR_INVALID_FORMAT;
    /* The algorithm is named twice, inside and outside what is signed. They must agree. */
    if (tbs_alg_whole.size != sig_alg_whole.size) return PROVEN_ERR_INVALID_FORMAT;
    for (proven_size_t i = 0; i < tbs_alg_whole.size; ++i) if (tbs_alg_whole.ptr[i] != sig_alg_whole.ptr[i]) return PROVEN_ERR_INVALID_FORMAT;
    parse_sig_alg(sig_alg, &c);
    if (!der_expect(&tbs, DER_SEQUENCE, NULL, &c.issuer)) return PROVEN_ERR_INVALID_FORMAT;
    if (!der_expect(&tbs, DER_SEQUENCE, &validity, NULL)) return PROVEN_ERR_INVALID_FORMAT;
    if (!der_time(&validity, &c.not_before) || !der_time(&validity, &c.not_after) || validity.n != 0) return PROVEN_ERR_INVALID_FORMAT;
    if (!der_expect(&tbs, DER_SEQUENCE, NULL, &c.subject)) return PROVEN_ERR_INVALID_FORMAT;
    if (!der_expect(&tbs, DER_SEQUENCE, &f, &spki_whole)) return PROVEN_ERR_INVALID_FORMAT;
    c.public_key_info = spki_whole;
    if (!parse_spki(f, &c)) return PROVEN_ERR_INVALID_FORMAT;
    /* issuerUniqueID [1] and subjectUniqueID [2]: implicit BIT STRINGs, skipped. */
    if (c.version >= 2) { (void)der_expect(&tbs, 0x81, &f, NULL); (void)der_expect(&tbs, 0x82, &f, NULL); }
    if (tbs.n != 0) {
        if (c.version != 3 || !der_expect(&tbs, 0xa3, &f, NULL) || tbs.n != 0) return PROVEN_ERR_INVALID_FORMAT;
        if (!parse_extensions(f, &c)) return PROVEN_ERR_INVALID_FORMAT;
    }
    *out = c;
    return PROVEN_OK;
}

/* ---- Names ---- */

bool proven_cert_alt_name_next(const proven_cert_t *cert, proven_size_t *pos,
                               proven_cert_name_kind_t *kind, proven_mem_view_t *value) {
    if (!cert || !pos || !kind || !value || *pos >= cert->alt_names.size) return false;
    proven_der_t in = { cert->alt_names.ptr + *pos, cert->alt_names.size - *pos };
    proven_byte_t tag;
    proven_der_t body;
    if (!der_read(&in, &tag, &body, NULL)) return false;
    *kind = tag == 0x82 ? PROVEN_CERT_NAME_DNS : tag == 0x87 ? PROVEN_CERT_NAME_IP : PROVEN_CERT_NAME_OTHER;
    value->ptr = body.p; value->size = body.n;
    *pos = cert->alt_names.size - in.n;
    return true;
}


static bool ascii_eq_nocase(const proven_byte_t *a, const proven_byte_t *b, proven_size_t n) {
    return proven_u8str_view_eq_ascii_ci(proven_u8str_view_from_bytes(a, n), proven_u8str_view_from_bytes(b, n));
}

static bool parse_ipv4(const proven_byte_t *s, proven_size_t n, proven_byte_t out[4]) {
    proven_size_t i = 0;
    for (int part = 0; part < 4; ++part) {
        proven_u32 v = 0;
        proven_size_t digits = 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            if (digits == 1 && v == 0) return false;                 /* no leading zeros */
            v = v * 10 + (proven_u32)(s[i] - '0');
            if (v > 255) return false;
            ++i; ++digits;
        }
        if (digits == 0) return false;
        out[part] = (proven_byte_t)v;
        if (part < 3) { if (i >= n || s[i] != '.') return false; ++i; }
    }
    return i == n;
}

static bool parse_ipv6(const proven_byte_t *s, proven_size_t n, proven_byte_t out[16]) {
    proven_u32 groups[8];
    int count = 0, gap = -1;
    proven_size_t i = 0;
    if (n >= 2 && s[0] == ':' && s[1] == ':') { gap = 0; i = 2; }
    else if (n >= 1 && s[0] == ':') return false;
    while (i < n) {
        proven_u32 v = 0;
        proven_size_t digits = 0;
        while (i < n && digits < 5) {
            proven_byte_t c = proven_ascii_lower(s[i]);
            if (c >= '0' && c <= '9') v = v * 16 + (proven_u32)(c - '0');
            else if (c >= 'a' && c <= 'f') v = v * 16 + (proven_u32)(c - 'a' + 10);
            else break;
            ++i; ++digits;
        }
        if (digits == 0 || digits > 4 || count >= 8) return false;
        groups[count++] = v;
        if (i == n) break;
        if (s[i] != ':') return false;
        ++i;
        if (i < n && s[i] == ':') {
            if (gap >= 0) return false;
            gap = count;
            ++i;
        } else if (i == n) {
            return false;                                            /* a single trailing colon */
        }
    }
    if (gap < 0 && count != 8) return false;
    if (gap >= 0 && count >= 8) return false;
    int fill = 8 - count, at = 0;
    for (int g = 0; g < 8; ++g) {
        proven_u32 v;
        if (gap >= 0 && g >= gap && g < gap + fill) v = 0;
        else v = groups[at++];
        out[2 * g] = (proven_byte_t)(v >> 8); out[2 * g + 1] = (proven_byte_t)v;
    }
    return true;
}

/* 4 or 16 when `host` is an IP literal, else 0. */
static proven_size_t host_as_ip(proven_u8str_view_t host, proven_byte_t out[16]) {
    if (parse_ipv4(host.ptr, host.size, out)) return 4;
    if (parse_ipv6(host.ptr, host.size, out)) return 16;
    return 0;
}

static bool dns_pattern_matches(const proven_byte_t *pat, proven_size_t pn, const proven_byte_t *host, proven_size_t hn) {
    if (pn == 0 || hn == 0) return false;
    if (pat[pn - 1] == '.') pn--;
    if (pn >= 2 && pat[0] == '*' && pat[1] == '.') {
        /* The rest must be a name of at least two labels, with no further wildcard. */
        bool dot = false;
        for (proven_size_t i = 2; i < pn; ++i) { if (pat[i] == '*') return false; if (pat[i] == '.') dot = true; }
        if (!dot) return false;
        proven_size_t first = 0;
        while (first < hn && host[first] != '.') ++first;
        if (first == 0 || first == hn) return false;
        return hn - first == pn - 1 && ascii_eq_nocase(host + first, pat + 1, pn - 1);
    }
    for (proven_size_t i = 0; i < pn; ++i) if (pat[i] == '*') return false;
    return pn == hn && ascii_eq_nocase(pat, host, pn);
}

/* For the TLS unit's certificate writer. Not public. */
proven_size_t proven_cert_host_as_ip_(proven_u8str_view_t text, proven_byte_t out[16]);
proven_size_t proven_cert_host_as_ip_(proven_u8str_view_t text, proven_byte_t out[16]) { return host_as_ip(text, out); }

bool proven_cert_matches_host(const proven_cert_t *cert, proven_u8str_view_t host) {
    if (!cert || !host.ptr || host.size == 0 || host.size > 253) return false;
    proven_byte_t ip[16];
    proven_size_t ip_len = host_as_ip(host, ip);
    proven_size_t hn = host.size;
    if (ip_len == 0 && host.ptr[hn - 1] == '.') hn--;
    proven_size_t pos = 0;
    proven_cert_name_kind_t kind;
    proven_mem_view_t v;
    while (proven_cert_alt_name_next(cert, &pos, &kind, &v)) {
        if (ip_len > 0) {
            if (kind != PROVEN_CERT_NAME_IP || v.size != ip_len) continue;
            bool same = true;
            for (proven_size_t i = 0; i < ip_len; ++i) if (v.ptr[i] != ip[i]) same = false;
            if (same) return true;
        } else if (kind == PROVEN_CERT_NAME_DNS) {
            if (dns_pattern_matches(v.ptr, v.size, host.ptr, hn)) return true;
        }
    }
    return false;
}

proven_err_t proven_cert_key_sha256(const proven_cert_t *cert, proven_byte_t out[32]) {
    if (!cert || !out || !cert->public_key_info.ptr) return PROVEN_ERR_INVALID_ARG;
    proven_sha256(cert->public_key_info, out);
    return PROVEN_OK;
}

/* ---- PEM ---- */

static bool starts_with(const proven_byte_t *p, proven_size_t n, const char *lit, proven_size_t ln) {
    if (n < ln) return false;
    for (proven_size_t i = 0; i < ln; ++i) if (p[i] != (proven_byte_t)lit[i]) return false;
    return true;
}

static int b64_value(proven_byte_t c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

proven_err_t proven_pem_next(proven_mem_view_t pem, proven_size_t *pos, proven_u8str_view_t *label,
                             proven_mem_mut_t out, proven_size_t *written) {
    if (!pem.ptr || !pos || !label || !written || (out.size > 0 && !out.ptr) || *pos > pem.size) return PROVEN_ERR_INVALID_ARG;
    const proven_byte_t *p = pem.ptr;
    proven_size_t n = pem.size, i = *pos;
    while (i < n && !starts_with(p + i, n - i, "-----BEGIN ", 11)) ++i;
    if (i >= n) return PROVEN_ERR_NOT_FOUND;
    proven_size_t label_at = i + 11, j = label_at;
    while (j < n && p[j] != '-' && p[j] != '\n' && p[j] != '\r') ++j;
    if (!starts_with(p + j, n - j, "-----", 5)) return PROVEN_ERR_INVALID_ENCODING;
    proven_size_t label_len = j - label_at, body_at = j + 5, k = body_at;
    /* The body runs to the matching END line. */
    while (k < n && p[k] != '-') ++k;
    if (!starts_with(p + k, n - k, "-----END ", 9) || n - k < 9 + label_len + 5) return PROVEN_ERR_INVALID_ENCODING;
    for (proven_size_t t = 0; t < label_len; ++t) if (p[k + 9 + t] != p[label_at + t]) return PROVEN_ERR_INVALID_ENCODING;
    if (!starts_with(p + k + 9 + label_len, n - k - 9 - label_len, "-----", 5)) return PROVEN_ERR_INVALID_ENCODING;
    /* Count the symbols first, so that a short buffer is reported with nothing written. */
    proven_size_t symbols = 0, padding = 0;
    for (proven_size_t t = body_at; t < k; ++t) {
        proven_byte_t c = p[t];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (c == '=') { padding++; continue; }
        if (padding > 0 || b64_value(c) < 0) return PROVEN_ERR_INVALID_ENCODING;
        symbols++;
    }
    if (padding > 2 || symbols % 4 == 1 || (padding > 0 && (symbols + padding) % 4 != 0)) return PROVEN_ERR_INVALID_ENCODING;
    proven_size_t need = symbols / 4 * 3 + (symbols % 4 == 0 ? 0 : symbols % 4 - 1);
    *written = need;
    if (need > out.size) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_u32 acc = 0;
    proven_size_t have = 0, at = 0;
    for (proven_size_t t = body_at; t < k; ++t) {
        int v = b64_value(p[t]);
        if (v < 0) continue;
        acc = (acc << 6) | (proven_u32)v;
        if (++have == 4) {
            out.ptr[at++] = (proven_byte_t)(acc >> 16); out.ptr[at++] = (proven_byte_t)(acc >> 8); out.ptr[at++] = (proven_byte_t)acc;
            acc = 0; have = 0;
        }
    }
    if (have == 2) {
        if (acc & 0x0f) return PROVEN_ERR_INVALID_ENCODING;          /* bits that belong to no byte */
        out.ptr[at++] = (proven_byte_t)(acc >> 4);
    } else if (have == 3) {
        if (acc & 0x03) return PROVEN_ERR_INVALID_ENCODING;
        out.ptr[at++] = (proven_byte_t)(acc >> 10); out.ptr[at++] = (proven_byte_t)(acc >> 2);
    }
    label->ptr = p + label_at; label->size = label_len;
    *pos = k + 9 + label_len + 5;
    return PROVEN_OK;
}

/* ---- The store ---- */

typedef struct {
    proven_byte_t *der;
    proven_cert_t cert;
} proven_store_entry_t;

struct proven_cert_store {
    proven_allocator_t alloc;
    proven_store_entry_t *items;
    proven_size_t count;
    proven_size_t cap;
};

proven_err_t proven_cert_store_create(proven_allocator_t alloc, proven_cert_store_t **out) {
    if (!out || !proven_alloc_is_valid(alloc)) return PROVEN_ERR_INVALID_ARG;
    proven_result_mem_mut_t m = alloc.alloc_fn(alloc.ctx, sizeof(proven_cert_store_t), 16);
    if (m.err != PROVEN_OK) return m.err;
    proven_cert_store_t *s = (proven_cert_store_t *)m.value.ptr;
    s->alloc = alloc; s->items = NULL; s->count = 0; s->cap = 0;
    *out = s;
    return PROVEN_OK;
}

void proven_cert_store_destroy(proven_cert_store_t *store) {
    if (!store) return;
    proven_allocator_t a = store->alloc;
    for (proven_size_t i = 0; i < store->count; ++i) a.free_fn(a.ctx, store->items[i].der);
    if (store->items) a.free_fn(a.ctx, store->items);
    a.free_fn(a.ctx, store);
}

proven_size_t proven_cert_store_count(const proven_cert_store_t *store) { return store ? store->count : 0; }

/* For cert_system.c, which reads files with the store's allocator. Not public. */
proven_allocator_t proven_cert_store_allocator_(const proven_cert_store_t *store);
proven_allocator_t proven_cert_store_allocator_(const proven_cert_store_t *store) { return store->alloc; }

proven_err_t proven_cert_store_add_der(proven_cert_store_t *store, proven_mem_view_t der) {
    if (!store || !der.ptr) return PROVEN_ERR_INVALID_ARG;
    proven_cert_t probe;
    proven_err_t e = proven_cert_parse(der, &probe);
    if (e != PROVEN_OK) return e;
    proven_allocator_t a = store->alloc;
    if (store->count == store->cap) {
        proven_size_t cap = store->cap ? store->cap * 2 : 16;
        if (cap > (proven_size_t)-1 / sizeof(proven_store_entry_t)) return PROVEN_ERR_NOMEM;
        proven_result_mem_mut_t m = store->items
            ? a.realloc_fn(a.ctx, store->items, store->cap * sizeof(proven_store_entry_t), cap * sizeof(proven_store_entry_t), 16)
            : a.alloc_fn(a.ctx, cap * sizeof(proven_store_entry_t), 16);
        if (m.err != PROVEN_OK) return m.err;
        store->items = (proven_store_entry_t *)m.value.ptr;
        store->cap = cap;
        /* The entries moved; their views point into the DER copies, which did not. */
    }
    proven_result_mem_mut_t m = a.alloc_fn(a.ctx, der.size, 1);
    if (m.err != PROVEN_OK) return m.err;
    proven_byte_t *copy = m.value.ptr;
    for (proven_size_t i = 0; i < der.size; ++i) copy[i] = der.ptr[i];
    proven_store_entry_t *it = &store->items[store->count];
    it->der = copy;
    e = proven_cert_parse((proven_mem_view_t){ .ptr = copy, .size = der.size }, &it->cert);
    if (e != PROVEN_OK) { a.free_fn(a.ctx, copy); return e; }
    store->count++;
    return PROVEN_OK;
}

proven_err_t proven_cert_store_add_pem(proven_cert_store_t *store, proven_mem_view_t pem, proven_size_t *added) {
    if (added) *added = 0;
    if (!store || !pem.ptr) return PROVEN_ERR_INVALID_ARG;
    proven_allocator_t a = store->alloc;
    /* No block decodes to more than three quarters of the text. */
    proven_size_t cap = pem.size / 4 * 3 + 3;
    proven_result_mem_mut_t m = a.alloc_fn(a.ctx, cap, 1);
    if (m.err != PROVEN_OK) return m.err;
    proven_size_t pos = 0, taken = 0;
    proven_err_t result = PROVEN_OK;
    for (;;) {
        proven_u8str_view_t label;
        proven_size_t len = 0;
        proven_err_t e = proven_pem_next(pem, &pos, &label, (proven_mem_mut_t){ .ptr = m.value.ptr, .size = cap }, &len);
        if (e == PROVEN_ERR_NOT_FOUND) break;
        if (e != PROVEN_OK) { result = e; break; }
        if (label.size != 11 || !starts_with(label.ptr, label.size, "CERTIFICATE", 11)) continue;
        e = proven_cert_store_add_der(store, (proven_mem_view_t){ .ptr = m.value.ptr, .size = len });
        if (e == PROVEN_OK) taken++;
        else if (e == PROVEN_ERR_NOMEM) { result = e; break; }
    }
    a.free_fn(a.ctx, m.value.ptr);
    if (added) *added = taken;
    if (result == PROVEN_OK && taken == 0) return PROVEN_ERR_NOT_FOUND;
    return result;
}

/* ---- Verification ---- */

static bool view_eq(proven_mem_view_t a, proven_mem_view_t b) {
    if (a.size != b.size) return false;
    for (proven_size_t i = 0; i < a.size; ++i) if (a.ptr[i] != b.ptr[i]) return false;
    return true;
}

static void digest_of(int hash, proven_mem_view_t data, proven_byte_t out[PROVEN_HMAC_MAX_SIZE]) {
    if (hash == PROVEN_HMAC_SHA256) proven_sha256(data, out);
    else if (hash == PROVEN_HMAC_SHA384) proven_sha384(data, out);
    else proven_sha512(data, out);
}

/* Whether `issuer`'s key signed `child`. ALGORITHM when the pair cannot be checked here. */
static proven_cert_fault_t check_signature(const proven_cert_t *child, const proven_cert_t *issuer) {
    proven_byte_t digest[PROVEN_HMAC_MAX_SIZE];
    proven_mem_view_t d = { .ptr = digest, .size = proven_hmac_size((proven_hmac_hash_t)child->sig_hash) };
    bool ok = false;
    switch (child->sig_kind) {
        case PROVEN_CERT_SIG_RSA_PKCS1:
            if (issuer->key_kind != PROVEN_CERT_KEY_RSA) return PROVEN_CERT_FAULT_BAD_SIGNATURE;
            digest_of(child->sig_hash, child->tbs, digest);
            ok = proven_crypto_rsa_verify_pkcs1(issuer->rsa_n, issuer->rsa_e, child->sig_hash, d, child->signature);
            break;
        case PROVEN_CERT_SIG_RSA_PSS:
            if (issuer->key_kind != PROVEN_CERT_KEY_RSA) return PROVEN_CERT_FAULT_BAD_SIGNATURE;
            digest_of(child->sig_hash, child->tbs, digest);
            ok = proven_crypto_rsa_verify_pss(issuer->rsa_n, issuer->rsa_e, child->sig_hash, child->sig_salt_len, d, child->signature);
            break;
        case PROVEN_CERT_SIG_ECDSA: {
            if (issuer->key_kind != PROVEN_CERT_KEY_EC_P256 && issuer->key_kind != PROVEN_CERT_KEY_EC_P384) return PROVEN_CERT_FAULT_BAD_SIGNATURE;
            digest_of(child->sig_hash, child->tbs, digest);
            ok = proven_crypto_ecdsa_verify_der(issuer->key_kind == PROVEN_CERT_KEY_EC_P256 ? PROVEN_CRYPTO_EC_P256 : PROVEN_CRYPTO_EC_P384,
                                                issuer->key, d, child->signature);
            break;
        }
        case PROVEN_CERT_SIG_ED25519:
            if (issuer->key_kind != PROVEN_CERT_KEY_ED25519 || child->signature.size != 64) return PROVEN_CERT_FAULT_BAD_SIGNATURE;
            ok = proven_crypto_ed25519_verify(issuer->key.ptr, child->tbs, child->signature.ptr);
            break;
        default:
            return PROVEN_CERT_FAULT_ALGORITHM;
    }
    return ok ? PROVEN_CERT_FAULT_NONE : PROVEN_CERT_FAULT_BAD_SIGNATURE;
}

/* Does `name` fall under the DNS constraint `c`? */
static bool dns_under(const proven_byte_t *name, proven_size_t nn, const proven_byte_t *c, proven_size_t cn) {
    if (cn == 0) return true;
    if (c[0] == '.') return nn > cn && ascii_eq_nocase(name + nn - cn, c, cn);
    if (nn == cn) return ascii_eq_nocase(name, c, cn);
    return nn > cn && name[nn - cn - 1] == '.' && ascii_eq_nocase(name + nn - cn, c, cn);
}

static bool ip_under(proven_mem_view_t ip, proven_mem_view_t c) {
    if (c.size != 2 * ip.size) return false;
    for (proven_size_t i = 0; i < ip.size; ++i) if ((ip.ptr[i] & c.ptr[ip.size + i]) != (c.ptr[i] & c.ptr[ip.size + i])) return false;
    return true;
}

/* One name against one list of subtrees. Sets *typed when the list has an entry of the
 * name's kind, *hit when one matches. False when the list is malformed. */
static bool subtrees_scan(proven_der_t list, proven_cert_name_kind_t kind, proven_mem_view_t name, bool excluded, bool *typed, bool *hit) {
    while (list.n > 0) {
        proven_der_t tree, base;
        proven_byte_t tag;
        if (!der_expect(&list, DER_SEQUENCE, &tree, NULL) || !der_read(&tree, &tag, &base, NULL)) return false;
        if (tree.n != 0) return false;                               /* minimum and maximum must be absent */
        if (tag == 0x82 && kind == PROVEN_CERT_NAME_DNS) {
            *typed = true;
            const proven_byte_t *n = name.ptr;
            proven_size_t nn = name.size;
            bool wild = nn >= 2 && n[0] == '*' && n[1] == '.';
            if (wild) { n += 2; nn -= 2; }
            if (dns_under(n, nn, base.p, base.n)) *hit = true;
            /* A wildcard can expand into an excluded subtree that lies below its own name. */
            if (excluded && wild && dns_under(base.p, base.n, n, nn)) *hit = true;
        } else if (tag == 0x87 && kind == PROVEN_CERT_NAME_IP) {
            *typed = true;
            if (ip_under(name, (proven_mem_view_t){ .ptr = base.p, .size = base.n })) *hit = true;
        }
    }
    return true;
}

/* The names of `below` against the nameConstraints of `ca`. Only DNS and IP names are
 * constrained here, because those are the only names this library asserts. */
static bool name_constraints_ok(const proven_cert_t *ca, const proven_cert_t *below) {
    proven_der_t in = { ca->name_constraints.ptr, ca->name_constraints.size }, permitted = { 0 }, excluded = { 0 };
    bool has_permitted = der_expect(&in, 0xa0, &permitted, NULL);
    bool has_excluded = der_expect(&in, 0xa1, &excluded, NULL);
    if (in.n != 0 || (!has_permitted && !has_excluded)) return false;
    proven_size_t pos = 0;
    proven_cert_name_kind_t kind;
    proven_mem_view_t v;
    while (proven_cert_alt_name_next(below, &pos, &kind, &v)) {
        if (kind == PROVEN_CERT_NAME_OTHER) continue;
        bool typed = false, hit = false;
        if (has_excluded) {
            if (!subtrees_scan(excluded, kind, v, true, &typed, &hit) || hit) return false;
        }
        typed = false; hit = false;
        if (has_permitted) {
            if (!subtrees_scan(permitted, kind, v, false, &typed, &hit)) return false;
            if (typed && !hit) return false;
        }
    }
    /* A list that does not parse must fail even when there is no name to hold against it. */
    bool t = false, h = false;
    proven_mem_view_t none = { .ptr = (const proven_byte_t *)"", .size = 0 };
    if (has_excluded && !subtrees_scan(excluded, PROVEN_CERT_NAME_OTHER, none, true, &t, &h)) return false;
    if (has_permitted && !subtrees_scan(permitted, PROVEN_CERT_NAME_OTHER, none, false, &t, &h)) return false;
    return true;
}

#define CV_MAX_EXTRA 16

typedef struct {
    const proven_cert_verify_options_t *opt;
    const proven_cert_t *extra;
    proven_size_t extra_count;
    bool used[CV_MAX_EXTRA];
    const proven_cert_t *path[PROVEN_CERT_MAX_DEPTH];
    proven_cert_fault_t fault;
    int rank;
    proven_size_t depth;
    proven_size_t budget;                 /* signature checks left: the search is bounded */
} proven_cv_t;

static int fault_rank(proven_cert_fault_t f) {
    switch (f) {
        case PROVEN_CERT_FAULT_NONE: return 9;
        case PROVEN_CERT_FAULT_EXPIRED:
        case PROVEN_CERT_FAULT_NOT_YET_VALID: return 5;
        case PROVEN_CERT_FAULT_NO_ISSUER: return 0;
        case PROVEN_CERT_FAULT_BAD_SIGNATURE:
        case PROVEN_CERT_FAULT_ALGORITHM: return 1;
        default: return 3;
    }
}

static void cv_note(proven_cv_t *cv, proven_cert_fault_t f) {
    if (fault_rank(f) > cv->rank) { cv->rank = fault_rank(f); cv->fault = f; }
}

/* A complete path: path[0] the leaf ... path[n-1] the anchor. Everything but signatures. */
static proven_cert_fault_t cv_check_path(const proven_cv_t *cv, proven_size_t n) {
    const proven_cert_use_t use = cv->opt->use;
    const proven_u32 want = use == PROVEN_CERT_USE_CLIENT ? PROVEN_CERT_EKU_CLIENT_AUTH
                          : use == PROVEN_CERT_USE_CODE_SIGNING ? PROVEN_CERT_EKU_CODE_SIGNING
                          : use == PROVEN_CERT_USE_TIMESTAMPING ? PROVEN_CERT_EKU_TIME_STAMPING : PROVEN_CERT_EKU_SERVER_AUTH;
    if (use == PROVEN_CERT_USE_CODE_SIGNING || use == PROVEN_CERT_USE_TIMESTAMPING) {
        /* The leaf must say the purpose itself - no extension is not "any" here - and be able to sign. */
        const proven_cert_t *leaf = cv->path[0];
        if (!leaf->has_ext_key_usage || !(leaf->ext_key_usage & want)) return PROVEN_CERT_FAULT_USAGE;
        if (use == PROVEN_CERT_USE_TIMESTAMPING && (!leaf->ext_key_usage_critical || leaf->ext_key_usage_count != 1)) return PROVEN_CERT_FAULT_USAGE;
        if (leaf->has_key_usage && !(leaf->key_usage & PROVEN_CERT_KU_DIGITAL_SIGNATURE)) return PROVEN_CERT_FAULT_USAGE;
    }
    for (proven_size_t i = 0; i + 1 < n; ++i) if (cv->path[i]->unknown_critical) return PROVEN_CERT_FAULT_CRITICAL_EXTENSION;
    for (proven_size_t i = 1; i < n; ++i) {
        const proven_cert_t *ca = cv->path[i];
        bool anchor = i == n - 1;
        /* A version 1 anchor has no extensions to say it is a CA; being an anchor says so. */
        if (!ca->is_ca && !(anchor && ca->version < 3)) return PROVEN_CERT_FAULT_NOT_A_CA;
        if (ca->has_key_usage && !(ca->key_usage & PROVEN_CERT_KU_KEY_CERT_SIGN)) return PROVEN_CERT_FAULT_NOT_A_CA;
        if (ca->has_path_len && i - 1 > ca->path_len) return PROVEN_CERT_FAULT_PATH_LENGTH;
        if (ca->name_constraints.size > 0) {
            for (proven_size_t j = 0; j < i; ++j) if (!name_constraints_ok(ca, cv->path[j])) return PROVEN_CERT_FAULT_NAME_CONSTRAINT;
        }
    }
    for (proven_size_t i = 0; i + 1 < n || i == 0; ++i) {
        const proven_cert_t *c = cv->path[i];
        if (c->has_ext_key_usage && !(c->ext_key_usage & (want | PROVEN_CERT_EKU_ANY))) return PROVEN_CERT_FAULT_USAGE;
        if (n == 1) break;
    }
    for (proven_size_t i = 0; i < n; ++i) {
        if (cv->opt->now < cv->path[i]->not_before) return PROVEN_CERT_FAULT_NOT_YET_VALID;
        if (cv->opt->now > cv->path[i]->not_after) return PROVEN_CERT_FAULT_EXPIRED;
    }
    return PROVEN_CERT_FAULT_NONE;
}

static bool cv_finish(proven_cv_t *cv, proven_size_t n) {
    proven_cert_fault_t f = cv_check_path(cv, n);
    cv_note(cv, f);
    if (f == PROVEN_CERT_FAULT_NONE) { cv->depth = n; return true; }
    return false;
}

static bool cv_search(proven_cv_t *cv, proven_size_t n) {
    const proven_cert_t *cur = cv->path[n - 1];
    const proven_cert_store_t *store = cv->opt->anchors;
    /* The certificate is itself an anchor. */
    for (proven_size_t i = 0; i < store->count; ++i) {
        if (view_eq(store->items[i].cert.der, cur->der)) {
            if (cv_finish(cv, n)) return true;
        }
    }
    if (n >= PROVEN_CERT_MAX_DEPTH) { cv_note(cv, PROVEN_CERT_FAULT_TOO_DEEP); return false; }
    for (proven_size_t i = 0; i < store->count; ++i) {
        const proven_cert_t *a = &store->items[i].cert;
        if (!view_eq(a->subject, cur->issuer)) continue;
        if (cv->budget == 0) return false;
        cv->budget--;
        proven_cert_fault_t f = check_signature(cur, a);
        if (f != PROVEN_CERT_FAULT_NONE) { cv_note(cv, f); continue; }
        cv->path[n] = a;
        if (cv_finish(cv, n + 1)) return true;
    }
    for (proven_size_t i = 0; i < cv->extra_count; ++i) {
        const proven_cert_t *c = &cv->extra[i];
        if (cv->used[i] || !view_eq(c->subject, cur->issuer)) continue;
        if (cv->budget == 0) return false;
        cv->budget--;
        proven_cert_fault_t f = check_signature(cur, c);
        if (f != PROVEN_CERT_FAULT_NONE) { cv_note(cv, f); continue; }
        cv->used[i] = true;
        cv->path[n] = c;
        if (cv_search(cv, n + 1)) return true;
        cv->used[i] = false;
    }
    return false;
}

proven_err_t proven_cert_verify(const proven_mem_view_t *chain, proven_size_t count,
                                const proven_cert_verify_options_t *options,
                                proven_cert_verify_result_t *result) {
    if (result) { result->fault = PROVEN_CERT_FAULT_NONE; result->depth = 0; }
    if (!chain || count == 0 || !options || !options->anchors || (options->host.size > 0 && !options->host.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_cert_t leaf, extra[CV_MAX_EXTRA];
    proven_cv_t cv = { 0 };
    if (proven_cert_parse(chain[0], &leaf) != PROVEN_OK) {
        if (result) result->fault = PROVEN_CERT_FAULT_MALFORMED;
        return PROVEN_ERR_INVALID_FORMAT;
    }
    /* What the peer sent besides its own certificate is only material for the search: one
     * that does not parse is one that cannot be used, not a reason to stop. */
    for (proven_size_t i = 1; i < count && cv.extra_count < CV_MAX_EXTRA; ++i) {
        if (chain[i].ptr && proven_cert_parse(chain[i], &extra[cv.extra_count]) == PROVEN_OK) cv.extra_count++;
    }
    cv.opt = options;
    cv.extra = extra;
    cv.fault = PROVEN_CERT_FAULT_NO_ISSUER;
    cv.rank = fault_rank(PROVEN_CERT_FAULT_NO_ISSUER);
    cv.budget = 64;
    cv.path[0] = &leaf;
    bool ok = cv_search(&cv, 1);
    proven_cert_fault_t fault = ok ? PROVEN_CERT_FAULT_NONE : cv.fault;
    if (ok && options->host.size > 0 && !proven_cert_matches_host(&leaf, options->host)) fault = PROVEN_CERT_FAULT_NAME_MISMATCH;
    if (result) { result->fault = fault; result->depth = ok ? cv.depth : 0; }
    switch (fault) {
        case PROVEN_CERT_FAULT_NONE: return PROVEN_OK;
        case PROVEN_CERT_FAULT_EXPIRED: return PROVEN_ERR_EXPIRED;
        case PROVEN_CERT_FAULT_NOT_YET_VALID: return PROVEN_ERR_NOT_YET_VALID;
        case PROVEN_CERT_FAULT_NAME_MISMATCH: return PROVEN_ERR_NAME_MISMATCH;
        default: return PROVEN_ERR_UNTRUSTED;
    }
}
