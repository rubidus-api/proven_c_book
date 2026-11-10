#include "proven/hmac.h"

/* HMAC (RFC 2104) and HKDF (RFC 5869). */

proven_size_t proven_hmac_size(proven_hmac_hash_t hash) {
    switch (hash) {
        case PROVEN_HMAC_SHA256: return PROVEN_SHA256_SIZE;
        case PROVEN_HMAC_SHA384: return PROVEN_SHA384_SIZE;
        case PROVEN_HMAC_SHA512: return PROVEN_SHA512_SIZE;
        case PROVEN_HMAC_SHA1: return PROVEN_SHA1_SIZE;
        default: return 0;
    }
}

static proven_size_t hmac_block_size(proven_hmac_hash_t hash) {
    return hash == PROVEN_HMAC_SHA256 || hash == PROVEN_HMAC_SHA1 ? 64 : 128;
}

static void hmac_hash_init(proven_hmac_t *h) {
    if (h->hash == PROVEN_HMAC_SHA256) proven_sha256_init(&h->inner.s256);
    else if (h->hash == PROVEN_HMAC_SHA1) proven_sha1_init(&h->inner.s1);
    else if (h->hash == PROVEN_HMAC_SHA384) proven_sha384_init(&h->inner.s512);
    else proven_sha512_init(&h->inner.s512);
}

static void hmac_hash_update(proven_hmac_t *h, proven_mem_view_t data) {
    if (h->hash == PROVEN_HMAC_SHA256) proven_sha256_update(&h->inner.s256, data);
    else if (h->hash == PROVEN_HMAC_SHA1) proven_sha1_update(&h->inner.s1, data);
    else proven_sha512_update(&h->inner.s512, data);
}

static void hmac_hash_final(proven_hmac_t *h, proven_byte_t out[PROVEN_HMAC_MAX_SIZE]) {
    if (h->hash == PROVEN_HMAC_SHA256) proven_sha256_final(&h->inner.s256, out);
    else if (h->hash == PROVEN_HMAC_SHA1) proven_sha1_final(&h->inner.s1, out);
    else if (h->hash == PROVEN_HMAC_SHA384) proven_sha384_final(&h->inner.s512, out);
    else proven_sha512_final(&h->inner.s512, out);
}

proven_err_t proven_hmac_init(proven_hmac_t *hmac, proven_hmac_hash_t hash, proven_mem_view_t key) {
    if (hmac) hmac->ready = false;
    if (!hmac || proven_hmac_size(hash) == 0 || (key.size > 0 && !key.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_size_t block = hmac_block_size(hash);
    proven_byte_t k[128];
    for (proven_size_t i = 0; i < sizeof k; ++i) k[i] = 0;
    hmac->hash = hash;
    if (key.size > block) {
        /* A key longer than a block is replaced by its digest. */
        proven_byte_t digest[PROVEN_HMAC_MAX_SIZE];
        hmac_hash_init(hmac);
        hmac_hash_update(hmac, key);
        hmac_hash_final(hmac, digest);
        for (proven_size_t i = 0; i < proven_hmac_size(hash); ++i) k[i] = digest[i];
        proven_mem_wipe((proven_mem_mut_t){ .ptr = digest, .size = sizeof digest });
    } else {
        for (proven_size_t i = 0; i < key.size; ++i) k[i] = key.ptr[i];
    }
    proven_byte_t ipad[128];
    for (proven_size_t i = 0; i < block; ++i) {
        ipad[i] = (proven_byte_t)(k[i] ^ 0x36);
        hmac->outer_key[i] = (proven_byte_t)(k[i] ^ 0x5c);
    }
    hmac_hash_init(hmac);
    hmac_hash_update(hmac, (proven_mem_view_t){ .ptr = ipad, .size = block });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = k, .size = sizeof k });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = ipad, .size = sizeof ipad });
    hmac->ready = true;
    return PROVEN_OK;
}

void proven_hmac_update(proven_hmac_t *hmac, proven_mem_view_t data) {
    if (!hmac || !hmac->ready) return;
    hmac_hash_update(hmac, data);
}

void proven_hmac_final(proven_hmac_t *hmac, proven_byte_t out[PROVEN_HMAC_MAX_SIZE]) {
    if (!hmac || !hmac->ready || !out) return;
    proven_size_t n = proven_hmac_size(hmac->hash);
    proven_byte_t inner[PROVEN_HMAC_MAX_SIZE];
    hmac_hash_final(hmac, inner);
    hmac_hash_init(hmac);
    hmac_hash_update(hmac, (proven_mem_view_t){ .ptr = hmac->outer_key, .size = hmac_block_size(hmac->hash) });
    hmac_hash_update(hmac, (proven_mem_view_t){ .ptr = inner, .size = n });
    proven_byte_t mac[PROVEN_HMAC_MAX_SIZE];
    hmac_hash_final(hmac, mac);
    for (proven_size_t i = 0; i < n; ++i) out[i] = mac[i];
    proven_mem_wipe((proven_mem_mut_t){ .ptr = inner, .size = sizeof inner });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = mac, .size = sizeof mac });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)hmac, .size = sizeof *hmac });
}

proven_err_t proven_hmac(proven_hmac_hash_t hash, proven_mem_view_t key, proven_mem_view_t data,
                         proven_byte_t out[PROVEN_HMAC_MAX_SIZE]) {
    if (!out || (data.size > 0 && !data.ptr)) return PROVEN_ERR_INVALID_ARG;
    proven_hmac_t h;
    proven_err_t e = proven_hmac_init(&h, hash, key);
    if (e != PROVEN_OK) return e;
    proven_hmac_update(&h, data);
    proven_hmac_final(&h, out);
    return PROVEN_OK;
}

proven_err_t proven_hkdf_extract(proven_hmac_hash_t hash, proven_mem_view_t salt, proven_mem_view_t ikm,
                                 proven_byte_t prk[PROVEN_HMAC_MAX_SIZE]) {
    if (!prk || (ikm.size > 0 && !ikm.ptr) || (salt.size > 0 && !salt.ptr)) return PROVEN_ERR_INVALID_ARG;
    /* An absent salt is HashLen zero bytes; as an HMAC key that is the same as an empty key. */
    return proven_hmac(hash, salt, ikm, prk);
}

proven_err_t proven_hkdf_expand(proven_hmac_hash_t hash, proven_mem_view_t prk, proven_mem_view_t info,
                                proven_mem_mut_t out) {
    proven_size_t n = proven_hmac_size(hash);
    if (n == 0 || !prk.ptr || prk.size < n || (info.size > 0 && !info.ptr) || (out.size > 0 && !out.ptr)) return PROVEN_ERR_INVALID_ARG;
    if (out.size > 255 * n) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_byte_t t[PROVEN_HMAC_MAX_SIZE];
    proven_size_t done = 0;
    proven_byte_t counter = 0;
    /* T(i) = HMAC(PRK, T(i-1) | info | i), and the output is T(1) | T(2) | ... */
    while (done < out.size) {
        proven_hmac_t h;
        proven_err_t e = proven_hmac_init(&h, hash, prk);
        if (e != PROVEN_OK) return e;
        if (counter > 0) proven_hmac_update(&h, (proven_mem_view_t){ .ptr = t, .size = n });
        proven_hmac_update(&h, info);
        counter++;
        proven_hmac_update(&h, (proven_mem_view_t){ .ptr = &counter, .size = 1 });
        proven_hmac_final(&h, t);
        proven_size_t take = out.size - done < n ? out.size - done : n;
        for (proven_size_t i = 0; i < take; ++i) out.ptr[done + i] = t[i];
        done += take;
    }
    proven_mem_wipe((proven_mem_mut_t){ .ptr = t, .size = sizeof t });
    return PROVEN_OK;
}

proven_err_t proven_hkdf(proven_hmac_hash_t hash, proven_mem_view_t salt, proven_mem_view_t ikm,
                         proven_mem_view_t info, proven_mem_mut_t out) {
    proven_size_t n = proven_hmac_size(hash);
    if (n == 0) return PROVEN_ERR_INVALID_ARG;
    if (out.size > 255 * n) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_byte_t prk[PROVEN_HMAC_MAX_SIZE];
    proven_err_t e = proven_hkdf_extract(hash, salt, ikm, prk);
    if (e == PROVEN_OK) e = proven_hkdf_expand(hash, (proven_mem_view_t){ .ptr = prk, .size = n }, info, out);
    proven_mem_wipe((proven_mem_mut_t){ .ptr = prk, .size = sizeof prk });
    return e;
}

/* RFC 8018 section 5.2. T_i = U_1 ^ U_2 ^ ... ^ U_c, U_1 = PRF(P, S | INT(i)), U_j = PRF(P, U_j-1).
 * The keyed state after the password is made once and copied for every PRF call: the key
 * schedule is two of the four compressions an HMAC of a short message costs. */
proven_err_t proven_pbkdf2(proven_hmac_hash_t hash, proven_mem_view_t password, proven_mem_view_t salt,
                           proven_u32 iterations, proven_mem_mut_t out) {
    const proven_size_t hlen = proven_hmac_size(hash);
    if (hlen == 0 || iterations == 0 || out.size == 0 || !out.ptr || (password.size > 0 && !password.ptr) || (salt.size > 0 && !salt.ptr))
        return PROVEN_ERR_INVALID_ARG;
    const proven_size_t blocks = out.size / hlen + (out.size % hlen != 0);
    if (blocks > 0xffffffffu) return PROVEN_ERR_OUT_OF_BOUNDS;
    proven_hmac_t keyed, h;
    proven_err_t e = proven_hmac_init(&keyed, hash, password);
    if (e != PROVEN_OK) return e;
    proven_byte_t u[PROVEN_HMAC_MAX_SIZE], t[PROVEN_HMAC_MAX_SIZE];
    for (proven_size_t b = 1, done = 0; b <= blocks; ++b) {
        const proven_byte_t index[4] = { (proven_byte_t)(b >> 24), (proven_byte_t)(b >> 16), (proven_byte_t)(b >> 8), (proven_byte_t)b };
        h = keyed;
        proven_hmac_update(&h, salt);
        proven_hmac_update(&h, (proven_mem_view_t){ .ptr = index, .size = 4 });
        proven_hmac_final(&h, u);
        for (proven_size_t i = 0; i < hlen; ++i) t[i] = u[i];
        for (proven_u32 j = 1; j < iterations; ++j) {
            h = keyed;
            proven_hmac_update(&h, (proven_mem_view_t){ .ptr = u, .size = hlen });
            proven_hmac_final(&h, u);
            for (proven_size_t i = 0; i < hlen; ++i) t[i] ^= u[i];
        }
        const proven_size_t take = out.size - done < hlen ? out.size - done : hlen;
        for (proven_size_t i = 0; i < take; ++i) out.ptr[done + i] = t[i];
        done += take;
    }
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&keyed, .size = sizeof keyed });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&h, .size = sizeof h });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = u, .size = sizeof u });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = t, .size = sizeof t });
    return PROVEN_OK;
}
