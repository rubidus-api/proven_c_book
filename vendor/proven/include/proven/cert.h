#ifndef PROVEN_CERT_H
#define PROVEN_CERT_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/allocator.h"
#include "proven/u8str.h"

/**
 * @file cert.h
 * @brief X.509 certificates: reading one, and deciding whether to believe it.
 *
 * A certificate is a signed statement: "this public key belongs to this name", signed by an
 * issuer whose own certificate says the same about the issuer, and so on up to a certificate
 * you already trust - a trust anchor. Verifying a peer means building that chain and checking
 * every link: the signature, the dates, that each issuer is allowed to issue, and at the end
 * that the name in the first certificate is the name you meant to reach.
 *
 *   - `proven_cert_parse` reads one certificate from DER. It is strict: definite lengths,
 *     minimal encodings, no trailing bytes.
 *   - `proven_cert_store_t` holds trust anchors: from a PEM bundle you supply, from DER, or
 *     from the operating system's store. The library carries no roots of its own.
 *   - `proven_cert_verify` builds and checks the chain.
 *
 * What is NOT checked, and you should know it: revocation (no CRL, no OCSP), Certificate
 * Transparency, and certificate policies. A certificate that was valid and has since been
 * revoked verifies here.
 *
 * Parsing and verification allocate nothing and call no OS: they are part of the freestanding
 * profile. A store allocates through the allocator you give it. `proven_cert_store_add_system`
 * is hosted only.
 */

/** @brief The longest chain `proven_cert_verify` will build, leaf and anchor included. */
#define PROVEN_CERT_MAX_DEPTH ((proven_size_t)10)

typedef enum {
    PROVEN_CERT_KEY_UNKNOWN = 0,     /**< An algorithm this library does not verify with. */
    PROVEN_CERT_KEY_RSA,
    PROVEN_CERT_KEY_EC_P256,
    PROVEN_CERT_KEY_EC_P384,
    PROVEN_CERT_KEY_ED25519
} proven_cert_key_kind_t;

typedef enum {
    PROVEN_CERT_SIG_UNKNOWN = 0,     /**< Includes SHA-1 and MD5 signatures: they are not accepted. */
    PROVEN_CERT_SIG_RSA_PKCS1,
    PROVEN_CERT_SIG_RSA_PSS,
    PROVEN_CERT_SIG_ECDSA,
    PROVEN_CERT_SIG_ED25519
} proven_cert_sig_kind_t;

/** @brief keyUsage bits, numbered as RFC 5280 numbers them. */
#define PROVEN_CERT_KU_DIGITAL_SIGNATURE ((proven_u32)1 << 0)
#define PROVEN_CERT_KU_KEY_ENCIPHERMENT  ((proven_u32)1 << 2)
#define PROVEN_CERT_KU_KEY_AGREEMENT     ((proven_u32)1 << 4)
#define PROVEN_CERT_KU_KEY_CERT_SIGN     ((proven_u32)1 << 5)
#define PROVEN_CERT_KU_CRL_SIGN          ((proven_u32)1 << 6)

/** @brief extendedKeyUsage purposes this library distinguishes. */
#define PROVEN_CERT_EKU_SERVER_AUTH ((proven_u32)1 << 0)
#define PROVEN_CERT_EKU_CLIENT_AUTH ((proven_u32)1 << 1)
#define PROVEN_CERT_EKU_ANY         ((proven_u32)1 << 2)
#define PROVEN_CERT_EKU_OTHER       ((proven_u32)1 << 3)   /**< A purpose not named here. */
#define PROVEN_CERT_EKU_CODE_SIGNING ((proven_u32)1 << 4)  /**< id-kp-codeSigning (since v0.109.0; OTHER before). */
#define PROVEN_CERT_EKU_TIME_STAMPING ((proven_u32)1 << 5) /**< id-kp-timeStamping (since v0.109.0; OTHER before). */

/**
 * @brief One parsed certificate. Every view points into the DER it was parsed from, which
 *        must outlive this struct. Nothing is owned.
 */
typedef struct {
    proven_mem_view_t der;               /**< The whole certificate. */
    proven_mem_view_t tbs;               /**< The signed part, tag and length included. */
    int               version;           /**< 1, 2 or 3. */
    proven_mem_view_t serial;            /**< The serial number's content bytes. */
    proven_mem_view_t issuer;            /**< The issuer Name, DER. */
    proven_mem_view_t subject;           /**< The subject Name, DER. */
    proven_i64        not_before;        /**< Seconds since 1970-01-01 UTC. */
    proven_i64        not_after;
    proven_mem_view_t public_key_info;   /**< SubjectPublicKeyInfo, DER: what a pin hashes. */
    proven_cert_key_kind_t key_kind;
    proven_mem_view_t key;               /**< EC: the point (04 | X | Y). Ed25519: 32 bytes. RSA: empty. */
    proven_mem_view_t rsa_n;             /**< RSA modulus, big-endian, no leading zero. */
    proven_mem_view_t rsa_e;             /**< RSA public exponent. */
    proven_cert_sig_kind_t sig_kind;     /**< How the ISSUER signed this certificate. */
    int               sig_hash;          /**< A proven_hmac_hash_t; unused for Ed25519. */
    proven_size_t     sig_salt_len;      /**< RSA-PSS only. */
    proven_mem_view_t signature;         /**< RSA: the integer. ECDSA: DER of r and s. Ed25519: 64 bytes. */
    bool              is_ca;             /**< basicConstraints says cA. */
    bool              has_path_len;
    proven_u32        path_len;
    bool              has_key_usage;
    proven_u32        key_usage;         /**< PROVEN_CERT_KU_* */
    bool              has_ext_key_usage;
    proven_u32        ext_key_usage;     /**< PROVEN_CERT_EKU_* */
    bool              ext_key_usage_critical; /**< extendedKeyUsage is marked critical (RFC 3161 asks it of a TSA). */
    proven_u32        ext_key_usage_count;    /**< How many purposes it lists. */
    proven_mem_view_t subject_key_id;    /**< subjectKeyIdentifier's octets; empty when absent. */
    proven_mem_view_t authority_key_id;  /**< authorityKeyIdentifier's keyIdentifier octets; empty when absent. */
    proven_mem_view_t alt_names;         /**< subjectAltName's GeneralNames content; empty when absent. */
    proven_mem_view_t name_constraints;  /**< nameConstraints content; empty when absent. */
    bool              unknown_critical;  /**< A critical extension this library does not understand. */
} proven_cert_t;

/**
 * @brief Parse one DER certificate.
 * @return PROVEN_ERR_INVALID_FORMAT for anything that is not a well-formed certificate,
 *         including trailing bytes; PROVEN_ERR_INVALID_ARG for a null argument. An unknown key
 *         or signature algorithm is not an error here: it parses, with the kind UNKNOWN, and
 *         fails later if verification needs it.
 */
[[nodiscard]]
proven_err_t proven_cert_parse(proven_mem_view_t der, proven_cert_t *out);

typedef enum {
    PROVEN_CERT_NAME_DNS = 0,
    PROVEN_CERT_NAME_IP,                 /**< 4 or 16 bytes. */
    PROVEN_CERT_NAME_OTHER               /**< Email, URI, directory name, ...: the raw content. */
} proven_cert_name_kind_t;

/**
 * @brief Walk the subjectAltName entries. Start with `*pos = 0`.
 * @return true and one entry, or false at the end (or on a malformed list).
 */
[[nodiscard]]
bool proven_cert_alt_name_next(const proven_cert_t *cert, proven_size_t *pos,
                               proven_cert_name_kind_t *kind, proven_mem_view_t *value);

/**
 * @brief Whether the certificate is for `host`: a DNS name or an IP address literal.
 *
 * Only subjectAltName is consulted - the subject's common name is not a host name. A DNS
 * pattern may have a wildcard as its whole left-most label (`*.example.com`), which matches
 * exactly one label and never a name with fewer than two labels after it. An IP literal is
 * matched against IP entries as an address, never against DNS entries.
 */
[[nodiscard]]
bool proven_cert_matches_host(const proven_cert_t *cert, proven_u8str_view_t host);

/**
 * @brief SHA-256 of the certificate's SubjectPublicKeyInfo: the value a public-key pin holds.
 * @return PROVEN_ERR_INVALID_ARG for a null argument.
 */
[[nodiscard]]
proven_err_t proven_cert_key_sha256(const proven_cert_t *cert, proven_byte_t out[32]);

/**
 * @brief Read the next PEM block from `pem`, starting at `*pos`.
 *
 * Text outside blocks is skipped. On success `*pos` is just past the block, `label` views the
 * words between "BEGIN " and the dashes (for a certificate, "CERTIFICATE"), and the decoded
 * bytes are in `out`.
 *
 * @return PROVEN_ERR_NOT_FOUND when there is no further block; PROVEN_ERR_INVALID_ENCODING for
 *         a block that does not end or whose body is not Base64; PROVEN_ERR_OUT_OF_BOUNDS when
 *         `out` is too small (`*written` is then the size needed and `*pos` has not moved).
 */
[[nodiscard]]
proven_err_t proven_pem_next(proven_mem_view_t pem, proven_size_t *pos, proven_u8str_view_t *label,
                             proven_mem_mut_t out, proven_size_t *written);

/** @brief A set of trust anchors. Owns copies of the certificates added to it. */
typedef struct proven_cert_store proven_cert_store_t;

/** @return PROVEN_ERR_INVALID_ARG, PROVEN_ERR_NOMEM. */
[[nodiscard]]
proven_err_t proven_cert_store_create(proven_allocator_t alloc, proven_cert_store_t **out);

/** @brief Free the store and every certificate in it. Null is accepted. */
void proven_cert_store_destroy(proven_cert_store_t *store);

/** @brief Add one DER certificate. @return PROVEN_ERR_INVALID_FORMAT, PROVEN_ERR_NOMEM. */
[[nodiscard]]
proven_err_t proven_cert_store_add_der(proven_cert_store_t *store, proven_mem_view_t der);

/**
 * @brief Add every certificate in a PEM bundle.
 *
 * A block that is not a certificate, and a certificate that does not parse, are skipped: a
 * system bundle is long, and one entry this library cannot read should not cost the rest.
 * `*added` (optional) counts the ones taken.
 *
 * @return PROVEN_ERR_NOMEM; PROVEN_ERR_INVALID_ENCODING for a bundle with a broken block;
 *         PROVEN_ERR_NOT_FOUND when the text holds no certificate at all.
 */
[[nodiscard]]
proven_err_t proven_cert_store_add_pem(proven_cert_store_t *store, proven_mem_view_t pem, proven_size_t *added);

#ifndef PROVEN_FREESTANDING
/**
 * @brief Add the operating system's trusted roots.
 *
 * On Windows this is the "ROOT" system store. Elsewhere it is the first readable file among
 * the path in the environment variable SSL_CERT_FILE and the usual bundle locations.
 *
 * @return PROVEN_ERR_NOT_FOUND when no store could be read; PROVEN_ERR_NOMEM.
 */
[[nodiscard]]
proven_err_t proven_cert_store_add_system(proven_cert_store_t *store, proven_size_t *added);
#endif

/** @brief How many anchors the store holds. 0 for null. */
proven_size_t proven_cert_store_count(const proven_cert_store_t *store);

typedef enum {
    PROVEN_CERT_USE_SERVER = 0,          /**< The peer is a server: its certificate must allow serverAuth. */
    PROVEN_CERT_USE_CLIENT,              /**< The peer is a client: clientAuth. */
    /** Code signing (Authenticode and the like), since v0.109.0: the leaf's extendedKeyUsage must
     *  be present and name codeSigning, its keyUsage (if any) allow digitalSignature; an issuer
     *  that lists purposes must allow codeSigning or any. Set `host` empty. */
    PROVEN_CERT_USE_CODE_SIGNING,
    /** A time-stamping authority (RFC 3161 section 2.3), since v0.109.0: the leaf's
     *  extendedKeyUsage must be critical and name timeStamping and nothing else; keyUsage as
     *  above; issuers as above. Set `host` empty. */
    PROVEN_CERT_USE_TIMESTAMPING
} proven_cert_use_t;

/** @brief Exactly what was wrong, for a log line. The error code is the coarse answer. */
typedef enum {
    PROVEN_CERT_FAULT_NONE = 0,
    PROVEN_CERT_FAULT_MALFORMED,         /**< A certificate in the chain does not parse. */
    PROVEN_CERT_FAULT_NO_ISSUER,         /**< No path from the leaf to any anchor. */
    PROVEN_CERT_FAULT_BAD_SIGNATURE,
    PROVEN_CERT_FAULT_ALGORITHM,         /**< A key or signature algorithm that is not accepted. */
    PROVEN_CERT_FAULT_NOT_A_CA,          /**< An issuer in the chain is not allowed to issue. */
    PROVEN_CERT_FAULT_PATH_LENGTH,
    PROVEN_CERT_FAULT_NAME_CONSTRAINT,
    PROVEN_CERT_FAULT_CRITICAL_EXTENSION,
    PROVEN_CERT_FAULT_USAGE,             /**< extendedKeyUsage does not allow this use. */
    PROVEN_CERT_FAULT_EXPIRED,
    PROVEN_CERT_FAULT_NOT_YET_VALID,
    PROVEN_CERT_FAULT_NAME_MISMATCH,
    PROVEN_CERT_FAULT_TOO_DEEP,
    PROVEN_CERT_FAULT_PIN_MISMATCH       /**< TLS only: the peer's key is not one of the configured pins. */
} proven_cert_fault_t;

typedef struct {
    const proven_cert_store_t *anchors;  /**< Required. */
    proven_u8str_view_t host;            /**< The name to match; empty to skip the name check. */
    proven_i64          now;             /**< Seconds since 1970-01-01 UTC. The caller supplies the time. */
    proven_cert_use_t   use;
} proven_cert_verify_options_t;

typedef struct {
    proven_cert_fault_t fault;
    proven_size_t       depth;           /**< Certificates in the chain that was accepted, anchor included. */
} proven_cert_verify_result_t;

/**
 * @brief Verify a peer's certificates.
 *
 * `chain[0]` is the peer's own certificate; the rest are whatever else it sent, in any order.
 * A path is searched from the leaf to an anchor in `options->anchors`; for every certificate
 * on it the signature, the validity period at `options->now`, the issuer's right to issue
 * (cA, keyCertSign, path length), name constraints and critical extensions are checked, and
 * then the leaf's name against `options->host`.
 *
 * @param result optional; receives the precise fault.
 * @return PROVEN_OK; PROVEN_ERR_UNTRUSTED when no acceptable path exists;
 *         PROVEN_ERR_EXPIRED / PROVEN_ERR_NOT_YET_VALID when a path exists but a certificate
 *         on it is outside its validity period; PROVEN_ERR_NAME_MISMATCH when the chain is
 *         good and the name is not; PROVEN_ERR_INVALID_FORMAT when the leaf does not parse;
 *         PROVEN_ERR_INVALID_ARG.
 */
[[nodiscard]]
proven_err_t proven_cert_verify(const proven_mem_view_t *chain, proven_size_t count,
                                const proven_cert_verify_options_t *options,
                                proven_cert_verify_result_t *result);

#endif /* PROVEN_CERT_H */
