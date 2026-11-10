#include "proven/aead.h"
#include "proven_internal_crypto.h"

/* aead.h: the AEADs and raw ciphers the TLS and QUIC engines use, given a public face. */

proven_err_t proven_aes_gcm_init(proven_aes_gcm_t *ctx, proven_mem_view_t key) {
    if (!ctx || !key.ptr || (key.size != 16 && key.size != 32)) return PROVEN_ERR_INVALID_ARG;
    return proven_crypto_aes_gcm_init(ctx, key.ptr, key.size) ? PROVEN_OK : PROVEN_ERR_INVALID_ARG;
}

void proven_aes_gcm_seal(const proven_aes_gcm_t *ctx, const proven_byte_t nonce[12], proven_mem_view_t aad,
                         proven_mem_view_t plain, proven_byte_t *out, proven_byte_t tag[16]) {
    proven_crypto_aes_gcm_seal(ctx, nonce, aad, plain, out, tag);
}

proven_err_t proven_aes_gcm_open(const proven_aes_gcm_t *ctx, const proven_byte_t nonce[12], proven_mem_view_t aad,
                                 proven_mem_view_t cipher, const proven_byte_t tag[16], proven_byte_t *out) {
    return proven_crypto_aes_gcm_open(ctx, nonce, aad, cipher, tag, out) ? PROVEN_OK : PROVEN_ERR_UNTRUSTED;
}

void proven_chacha20_poly1305_seal(const proven_byte_t key[32], const proven_byte_t nonce[12], proven_mem_view_t aad,
                                   proven_mem_view_t plain, proven_byte_t *out, proven_byte_t tag[16]) {
    proven_crypto_chacha20poly1305_seal(key, nonce, aad, plain, out, tag);
}

proven_err_t proven_chacha20_poly1305_open(const proven_byte_t key[32], const proven_byte_t nonce[12], proven_mem_view_t aad,
                                           proven_mem_view_t cipher, const proven_byte_t tag[16], proven_byte_t *out) {
    return proven_crypto_chacha20poly1305_open(key, nonce, aad, cipher, tag, out) ? PROVEN_OK : PROVEN_ERR_UNTRUSTED;
}

void proven_chacha20(const proven_byte_t key[32], proven_u32 counter, const proven_byte_t nonce[12],
                     const proven_byte_t *in, proven_byte_t *out, proven_size_t len) {
    proven_crypto_chacha20(key, counter, nonce, in, out, len);
}

proven_err_t proven_aes_init(proven_aes_t *ctx, proven_mem_view_t key) {
    if (!ctx || !key.ptr || (key.size != 16 && key.size != 32)) return PROVEN_ERR_INVALID_ARG;
    return proven_crypto_aes_init(ctx, key.ptr, key.size) ? PROVEN_OK : PROVEN_ERR_INVALID_ARG;
}

void proven_aes_encrypt_block(const proven_aes_t *ctx, const proven_byte_t in[16], proven_byte_t out[16]) {
    proven_crypto_aes_block(ctx, in, out);
}

bool proven_aes_hw_available(void) { return proven_crypto_aes_hw_available(); }
