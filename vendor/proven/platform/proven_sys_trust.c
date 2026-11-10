#include "proven_sys_trust.h"

#if defined(_WIN32)

#include <windows.h>
#include <wincrypt.h>

int proven_sys_trust_enumerate(proven_sys_trust_visit_fn visit, void *ctx) {
    HCERTSTORE store = CertOpenSystemStoreW(0, L"ROOT");
    if (!store) return 0;
    PCCERT_CONTEXT cert = NULL;
    while ((cert = CertEnumCertificatesInStore(store, cert)) != NULL) {
        if ((cert->dwCertEncodingType & X509_ASN_ENCODING) == 0) continue;
        if (!visit(ctx, cert->pbCertEncoded, (size_t)cert->cbCertEncoded)) {
            CertFreeCertificateContext(cert);
            break;
        }
    }
    CertCloseStore(store, 0);
    return 1;
}

const char *proven_sys_trust_bundle_path(size_t index) {
    (void)index;
    return NULL;
}

#else

int proven_sys_trust_enumerate(proven_sys_trust_visit_fn visit, void *ctx) {
    (void)visit; (void)ctx;
    return -1;
}

const char *proven_sys_trust_bundle_path(size_t index) {
    /* Debian and Ubuntu, Arch and Gentoo; Fedora and RHEL; openSUSE; the BSDs, macOS and
     * Alpine; FreeBSD's port; NetBSD. */
    static const char *const paths[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
        "/etc/ssl/ca-bundle.pem",
        "/etc/ssl/cert.pem",
        "/usr/local/share/certs/ca-root-nss.crt",
        "/etc/openssl/certs/ca-certificates.crt",
    };
    return index < sizeof paths / sizeof paths[0] ? paths[index] : NULL;
}

#endif
