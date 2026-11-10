#include "proven/kex.h"
#include "proven_internal_crypto.h"

/* kex.h: X25519 and ECDH over the TLS engine's curve code. Finite-field DH is dh.c. */

void proven_x25519_public(proven_byte_t out[32], const proven_byte_t scalar[32]) {
    proven_crypto_x25519_public(out, scalar);
}

proven_err_t proven_x25519(proven_byte_t out[32], const proven_byte_t scalar[32], const proven_byte_t peer[32]) {
    if (!out || !scalar || !peer) return PROVEN_ERR_INVALID_ARG;
    return proven_crypto_x25519(out, scalar, peer) ? PROVEN_OK : PROVEN_ERR_UNTRUSTED;
}

static bool kex_curve(proven_pkey_kind_t kind, proven_crypto_ec_curve_t *curve, proven_size_t *size) {
    if (kind == PROVEN_PKEY_P256) { *curve = PROVEN_CRYPTO_EC_P256; *size = 32; return true; }
    if (kind == PROVEN_PKEY_P384) { *curve = PROVEN_CRYPTO_EC_P384; *size = 48; return true; }
    return false;
}

proven_err_t proven_ec_public(proven_pkey_kind_t kind, proven_mem_view_t scalar, proven_mem_mut_t out) {
    proven_crypto_ec_curve_t curve;
    proven_size_t size;
    if (!kex_curve(kind, &curve, &size) || !scalar.ptr || scalar.size != size || !out.ptr) return PROVEN_ERR_INVALID_ARG;
    if (out.size < 1 + 2 * size) return PROVEN_ERR_OUT_OF_BOUNDS;
    return proven_crypto_ec_public(curve, scalar.ptr, out.ptr) ? PROVEN_OK : PROVEN_ERR_INVALID_FORMAT;
}

proven_err_t proven_ecdh(proven_pkey_kind_t kind, proven_mem_view_t scalar, proven_mem_view_t peer, proven_mem_mut_t out) {
    proven_crypto_ec_curve_t curve;
    proven_size_t size;
    if (!kex_curve(kind, &curve, &size) || !scalar.ptr || scalar.size != size || !out.ptr || (peer.size > 0 && !peer.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (out.size < size) return PROVEN_ERR_OUT_OF_BOUNDS;
    return proven_crypto_ecdh(curve, scalar.ptr, peer, out.ptr) ? PROVEN_OK : PROVEN_ERR_INVALID_FORMAT;
}
