#ifndef PROVEN_INTERNAL_CERT_ISSUE_H
#define PROVEN_INTERNAL_CERT_ISSUE_H

/* Writing certificates and keys (cert_issue.c): DER and PEM out, the mirror of cert.c's reader.
 * Internal, for tests that need an identity or a chain - the base library's and the network
 * library's alike; no library code calls it. The proven_tls_ names are those of the tests that
 * first used it, when it lived beside the TLS engine (RFC-0043 moved it here). */

#include "proven/types.h"
#include "proven/memory.h"
#include "proven/u8str.h"
#include "proven/cert.h"
#include "proven_internal_crypto.h"

typedef enum { PROVEN_ISSUE_KEY_NONE = 0, PROVEN_ISSUE_KEY_P256, PROVEN_ISSUE_KEY_ED25519, PROVEN_ISSUE_KEY_RSA } proven_issue_key_t;
/* Fill `out` with `len` unpredictable bytes (the shape of tls.h's proven_tls_random_fn). */
typedef void (*proven_issue_random_fn)(void *ctx, proven_byte_t *out, proven_size_t len);

/* ---- Issuing certificates: for tests that need an identity or a chain ---- */

/* A self-signed Ed25519 certificate for `names` (DNS names or IP literals, 1 to 16), valid from
 * `not_before` to `not_after`, for server and client authentication, and its key, as PEM. Public
 * as proven_tls_self_signed until v0.78.0 (owner decision 2026-10-23: an identity comes from
 * files). random NULL: the system's (hosted). PROVEN_ERR_INVALID_ARG, PROVEN_ERR_OUT_OF_BOUNDS. */
proven_err_t proven_tls_self_signed_(const proven_u8str_view_t *names, proven_size_t name_count,
                                     proven_i64 not_before, proven_i64 not_after,
                                     proven_issue_random_fn random, void *random_ctx,
                                     proven_mem_mut_t certificate_pem, proven_size_t *certificate_len,
                                     proven_mem_mut_t private_key_pem, proven_size_t *private_key_len);

typedef struct {
    const char *subject;                          /* the subject's common name */
    const char *issuer;                           /* the issuer's; the same string for a self-signed certificate */
    proven_issue_key_t subject_kind;
    const proven_byte_t *subject_key;             /* 32 bytes: an Ed25519 seed or a P-256 scalar */
    proven_issue_key_t issuer_kind;
    const proven_byte_t *issuer_key;
    const proven_crypto_rsa_key_t *subject_rsa;   /* instead of subject_key, for PROVEN_ISSUE_KEY_RSA */
    const proven_crypto_rsa_key_t *issuer_rsa;
    bool is_ca;
    proven_u32 eku;                               /* PROVEN_CERT_EKU_SERVER_AUTH, _CLIENT_AUTH, _CODE_SIGNING, _TIME_STAMPING; 0: none */
    bool eku_critical;                            /* extendedKeyUsage marked critical (a TSA's, RFC 3161) */
    proven_mem_view_t ski, aki;                   /* subjectKeyIdentifier and authorityKeyIdentifier's keyIdentifier; empty: none */
    const proven_u8str_view_t *names;             /* subjectAltName entries: DNS names and IP literals */
    proven_size_t name_count;
    proven_i64 not_before, not_after;
    proven_byte_t serial[16];
} proven_tls_issue_t;

/* One certificate, DER. PROVEN_ERR_OUT_OF_BOUNDS when `out` is too small (1 KiB is plenty for a few names). */
proven_err_t proven_tls_issue_(const proven_tls_issue_t *what, proven_mem_mut_t out, proven_size_t *len);
/* A private key as PKCS #8 DER; with `sec1`, a P-256 key as an ECPrivateKey instead. At most 80 bytes. */
proven_size_t proven_tls_key_der_(proven_issue_key_t kind, const proven_byte_t key[32], bool sec1, proven_byte_t out[80]);
/* An RSA private key as an RSAPrivateKey (PKCS #1), or with `pkcs8` inside a PrivateKeyInfo.
 * `d` is the private exponent, n_len bytes. Returns the length, or 0 when `out` is too small. */
proven_size_t proven_tls_rsa_key_der_(const proven_crypto_rsa_key_t *key, const proven_byte_t *d, bool pkcs8, proven_mem_mut_t out);
/* PEM: the label between the dashes, the bytes in Base64, 64 to a line. */
proven_err_t proven_tls_pem_write_(const char *label, proven_mem_view_t der, proven_mem_mut_t out, proven_size_t *len);
/* 4 or 16 when `text` is an IP literal (cert.c). */
proven_size_t proven_cert_host_as_ip_(proven_u8str_view_t text, proven_byte_t out[16]);

#endif
