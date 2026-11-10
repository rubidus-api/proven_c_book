#include "proven/aes.h"
#include "proven_internal_crypto.h"

/* aes.h: CBC over the TLS engine's AES, and PKCS #7 padding. */

static proven_err_t aes_cbc(proven_mem_view_t key, proven_byte_t iv[16], proven_mem_mut_t data, bool decrypt) {
    if (!key.ptr || !iv || (data.size > 0 && !data.ptr) || data.size % 16 != 0) return PROVEN_ERR_INVALID_ARG;
    proven_crypto_aes_gcm_t ctx;
    if (!proven_crypto_aes_cbc_init(&ctx, key.ptr, key.size)) return PROVEN_ERR_INVALID_ARG;
    if (decrypt) proven_crypto_aes_cbc_decrypt(&ctx, iv, data.ptr, data.size);
    else proven_crypto_aes_cbc_encrypt(&ctx, iv, data.ptr, data.size);
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&ctx, .size = sizeof ctx });
    return PROVEN_OK;
}

proven_err_t proven_aes_cbc_decrypt(proven_mem_view_t key, proven_byte_t iv[16], proven_mem_mut_t data) {
    return aes_cbc(key, iv, data, true);
}

proven_err_t proven_aes_cbc_encrypt(proven_mem_view_t key, proven_byte_t iv[16], proven_mem_mut_t data) {
    return aes_cbc(key, iv, data, false);
}

proven_err_t proven_aes_cbc_key_init(proven_aes_cbc_key_t *ctx, proven_mem_view_t key) {
    if (!ctx || !key.ptr) return PROVEN_ERR_INVALID_ARG;
    return proven_crypto_aes_cbc_init(&ctx->k, key.ptr, key.size) ? PROVEN_OK : PROVEN_ERR_INVALID_ARG;
}

static proven_err_t aes_cbc_with(const proven_aes_cbc_key_t *ctx, proven_byte_t iv[16], proven_mem_mut_t data, bool decrypt) {
    if (!ctx || !iv || (data.size > 0 && !data.ptr) || data.size % 16 != 0) return PROVEN_ERR_INVALID_ARG;
    if (decrypt) proven_crypto_aes_cbc_decrypt(&ctx->k, iv, data.ptr, data.size);
    else proven_crypto_aes_cbc_encrypt(&ctx->k, iv, data.ptr, data.size);
    return PROVEN_OK;
}

proven_err_t proven_aes_cbc_decrypt_with(const proven_aes_cbc_key_t *ctx, proven_byte_t iv[16], proven_mem_mut_t data) {
    return aes_cbc_with(ctx, iv, data, true);
}

proven_err_t proven_aes_cbc_encrypt_with(const proven_aes_cbc_key_t *ctx, proven_byte_t iv[16], proven_mem_mut_t data) {
    return aes_cbc_with(ctx, iv, data, false);
}

proven_err_t proven_pkcs7_unpad(proven_mem_view_t data, proven_size_t block, proven_size_t *len) {
    if (len) *len = 0;
    if (!len || block == 0 || block > 255 || !data.ptr || data.size == 0 || data.size % block != 0) return PROVEN_ERR_INVALID_ARG;
    /* Every byte of the last block is looked at, and the verdict gathered without a branch on
     * it, so how far the check got does not show in its time. */
    const proven_byte_t *last = data.ptr + data.size - block;
    const proven_u32 pad = data.ptr[data.size - 1];
    proven_u32 bad = (proven_u32)((pad - 1u) >> 31) | (proven_u32)((block - (proven_size_t)pad) >> (sizeof(proven_size_t) * 8 - 1));
    for (proven_size_t i = 0; i < block; ++i) {
        const proven_u32 in_pad = (proven_u32)(((proven_u32)(block - 1 - i) - pad) >> 31);   /* 1 for the last `pad` bytes */
        bad |= in_pad & (proven_u32)(last[i] != pad);
    }
    if (bad) return PROVEN_ERR_INVALID_FORMAT;
    *len = data.size - pad;
    return PROVEN_OK;
}
