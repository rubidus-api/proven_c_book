#include "proven_internal_crypto.h"
#include "proven/hash.h"
#include "proven/hmac.h"
#include "proven/memory.h"
#include "proven_internal_der.h"

/* RSA signatures: PKCS #1 v1.5 and PSS. Verification first - a public operation - then
 * signing, where everything about the key but n and e is secret. */

#define RSA_MAX_BYTES (PROVEN_CRYPTO_RSA_MAX_BITS / 8)

/* The smallest modulus accepted. A test lowers it to replay RFC 8448, whose example keys are
 * 1024 bits; nothing else may. */
static proven_size_t g_rsa_min_bits = PROVEN_CRYPTO_RSA_MIN_BITS;
void proven_crypto_rsa_test_min_bits(proven_size_t bits);
void proven_crypto_rsa_test_min_bits(proven_size_t bits) { g_rsa_min_bits = bits ? bits : PROVEN_CRYPTO_RSA_MIN_BITS; }
proven_size_t proven_crypto_rsa_min_bits(void) { return g_rsa_min_bits; }

#define RSA_E_LIMBS PROVEN_CRYPTO_MP_LIMBS(8)                    /* a public exponent: 64 bits at most */

/* sig^e mod n into `em`, exactly as many bytes as the modulus. Returns that length, or 0. */
static proven_size_t rsa_public(proven_mem_view_t n, proven_mem_view_t e, proven_mem_view_t sig,
                                proven_byte_t em[RSA_MAX_BYTES], proven_size_t *bits_out) {
    proven_crypto_mp_mod_t mod;
    proven_crypto_limb_t s[PROVEN_CRYPTO_MP_MAX], ex[RSA_E_LIMBS];
    if (!n.ptr || !e.ptr || !sig.ptr) return 0;
    while (n.size > 0 && n.ptr[0] == 0) { n.ptr++; n.size--; }
    while (e.size > 0 && e.ptr[0] == 0) { e.ptr++; e.size--; }
    if (n.size > RSA_MAX_BYTES || e.size == 0 || e.size > 8) return 0;
    if (!proven_crypto_mp_mod_init(&mod, n.ptr, n.size)) return 0;
    if (mod.bits < g_rsa_min_bits || mod.bits > PROVEN_CRYPTO_RSA_MAX_BITS) return 0;
    if (!proven_crypto_mp_load_be(ex, RSA_E_LIMBS, e.ptr, e.size)) return 0;
    if ((ex[0] & 1u) == 0 || (e.size == 1 && e.ptr[0] < 3)) return 0;      /* e has no leading zero byte here */
    if (sig.size != n.size) return 0;
    if (!proven_crypto_mp_load_be(s, mod.limbs, sig.ptr, sig.size)) return 0;
    if (!proven_crypto_mp_lt(s, mod.n, mod.limbs)) return 0;
    proven_crypto_mp_to_mont(&mod, s, s);
    proven_crypto_mp_pow(&mod, s, s, ex, RSA_E_LIMBS);
    proven_crypto_mp_from_mont(&mod, s, s);
    proven_crypto_mp_store_be(em, n.size, s, mod.limbs);
    *bits_out = mod.bits;
    return n.size;
}

static const proven_byte_t RSA_DI_SHA256[] = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20 };
static const proven_byte_t RSA_DI_SHA384[] = { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30 };
static const proven_byte_t RSA_DI_SHA512[] = { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40 };
static const proven_byte_t RSA_DI_SHA1[] = { 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14 };

/* The DigestInfo that goes before a digest of `hash`, and its length; null for another hash. */
static const proven_byte_t *rsa_digest_info(int hash, proven_size_t *len) {
    *len = 19;
    if (hash == PROVEN_HMAC_SHA256) return RSA_DI_SHA256;
    if (hash == PROVEN_HMAC_SHA384) return RSA_DI_SHA384;
    if (hash == PROVEN_HMAC_SHA512) return RSA_DI_SHA512;
    *len = sizeof RSA_DI_SHA1;
    if (hash == PROVEN_HMAC_SHA1) return RSA_DI_SHA1;
    return (const proven_byte_t *)0;
}

bool proven_crypto_rsa_verify_pkcs1(proven_mem_view_t n, proven_mem_view_t e, int hash,
                                    proven_mem_view_t digest, proven_mem_view_t sig) {
    proven_byte_t em[RSA_MAX_BYTES];
    proven_size_t bits = 0;
    const proven_byte_t *di;
    proven_size_t hlen = proven_hmac_size((proven_hmac_hash_t)hash);
    proven_size_t di_len = 0;
    if (!(di = rsa_digest_info(hash, &di_len))) return false;
    if (!digest.ptr || digest.size != hlen) return false;
    proven_size_t k = rsa_public(n, e, sig, em, &bits);
    if (k == 0) return false;
    /* The whole encoding is rebuilt and compared: 00 01 FF..FF 00 DigestInfo. Parsing what the
     * signature decrypts to is how forgeries with trailing garbage have been accepted. */
    const proven_size_t t_len = di_len + hlen;
    if (k < t_len + 11) return false;
    proven_byte_t diff = 0;
    proven_size_t pad_end = k - t_len - 1;
    diff |= em[0];
    diff |= (proven_byte_t)(em[1] ^ 0x01);
    for (proven_size_t i = 2; i < pad_end; ++i) diff |= (proven_byte_t)(em[i] ^ 0xff);
    diff |= em[pad_end];
    for (proven_size_t i = 0; i < di_len; ++i) diff |= (proven_byte_t)(em[pad_end + 1 + i] ^ di[i]);
    for (proven_size_t i = 0; i < hlen; ++i) diff |= (proven_byte_t)(em[pad_end + 1 + di_len + i] ^ digest.ptr[i]);
    return diff == 0;
}

static void rsa_hash(int hash, proven_mem_view_t a, proven_mem_view_t b, proven_mem_view_t c, proven_byte_t out[PROVEN_HMAC_MAX_SIZE]) {
    if (hash == PROVEN_HMAC_SHA1) {
        proven_sha1_t h;
        proven_sha1_init(&h);
        proven_sha1_update(&h, a); proven_sha1_update(&h, b); proven_sha1_update(&h, c);
        proven_sha1_final(&h, out);
    } else if (hash == PROVEN_HMAC_SHA256) {
        proven_sha256_t h;
        proven_sha256_init(&h);
        proven_sha256_update(&h, a); proven_sha256_update(&h, b); proven_sha256_update(&h, c);
        proven_sha256_final(&h, out);
    } else {
        proven_sha512_t h;
        if (hash == PROVEN_HMAC_SHA384) proven_sha384_init(&h); else proven_sha512_init(&h);
        proven_sha512_update(&h, a); proven_sha512_update(&h, b); proven_sha512_update(&h, c);
        if (hash == PROVEN_HMAC_SHA384) proven_sha384_final(&h, out); else proven_sha512_final(&h, out);
    }
}

bool proven_crypto_rsa_verify_pss(proven_mem_view_t n, proven_mem_view_t e, int hash, proven_size_t salt_len,
                                  proven_mem_view_t digest, proven_mem_view_t sig) {
    proven_byte_t em_full[RSA_MAX_BYTES], db[RSA_MAX_BYTES], h2[PROVEN_HMAC_MAX_SIZE], block[PROVEN_HMAC_MAX_SIZE];
    proven_size_t bits = 0;
    proven_size_t hlen = proven_hmac_size((proven_hmac_hash_t)hash);
    if (hlen == 0 || !digest.ptr || digest.size != hlen || salt_len > RSA_MAX_BYTES) return false;
    proven_size_t k = rsa_public(n, e, sig, em_full, &bits);
    if (k == 0) return false;
    /* EM is emLen = ceil((bits - 1) / 8) bytes: one shorter than the modulus when bits = 1 mod 8. */
    proven_size_t em_bits = bits - 1;
    proven_size_t em_len = (em_bits + 7) / 8;
    const proven_byte_t *em = em_full + (k - em_len);
    if (em_len < k && em_full[0] != 0) return false;
    if (em_len < hlen + salt_len + 2) return false;
    if (em[em_len - 1] != 0xbc) return false;
    proven_size_t db_len = em_len - hlen - 1;
    const proven_byte_t *h = em + db_len;
    proven_byte_t top_mask = (proven_byte_t)(0xffu >> (8 * em_len - em_bits));
    if ((em[0] & (proven_byte_t)~top_mask) != 0) return false;
    /* DB = maskedDB xor MGF1(H) */
    for (proven_size_t done = 0, counter = 0; done < db_len; ++counter) {
        proven_byte_t c[4] = { (proven_byte_t)(counter >> 24), (proven_byte_t)(counter >> 16), (proven_byte_t)(counter >> 8), (proven_byte_t)counter };
        rsa_hash(hash, (proven_mem_view_t){ .ptr = h, .size = hlen }, (proven_mem_view_t){ .ptr = c, .size = 4 }, (proven_mem_view_t){ .ptr = c, .size = 0 }, block);
        for (proven_size_t i = 0; i < hlen && done < db_len; ++i, ++done) db[done] = (proven_byte_t)(em[done] ^ block[i]);
    }
    db[0] &= top_mask;
    proven_size_t ps_len = db_len - salt_len - 1;
    proven_byte_t diff = 0;
    for (proven_size_t i = 0; i < ps_len; ++i) diff |= db[i];
    diff |= (proven_byte_t)(db[ps_len] ^ 0x01);
    if (diff != 0) return false;
    static const proven_byte_t zeros[8] = { 0 };
    rsa_hash(hash, (proven_mem_view_t){ .ptr = zeros, .size = 8 }, digest, (proven_mem_view_t){ .ptr = db + ps_len + 1, .size = salt_len }, h2);
    return proven_mem_equal_ct((proven_mem_view_t){ .ptr = h2, .size = hlen }, (proven_mem_view_t){ .ptr = h, .size = hlen });
}

// -----------------------------------------------------------------------------
// Signing
// -----------------------------------------------------------------------------
//
// The private operation is done modulo the two primes and recombined (CRT): four times less
// work than one exponentiation modulo n. Three things guard it:
//
//   - every step on secret data is constant-time: the exponentiation is
//     proven_crypto_mp_pow_ct, reductions are bit-serial under masks, and nothing branches
//     on a prime, an exponent or an intermediate;
//   - the input is blinded when a pair is given, so that what is exponentiated is not a
//     value anyone outside chose;
//   - the result is checked with the public key before it leaves. A CRT signature that is
//     right modulo one prime and wrong modulo the other gives the modulus's factors to anyone
//     who sees it, so a result that does not verify is never returned.

#define RSA_SIGN_BYTES PROVEN_CRYPTO_RSA_SIGN_MAX_BYTES
#define RSA_PRIME_BYTES PROVEN_CRYPTO_RSA_PRIME_MAX_BYTES
#define RSA_SIGN_LIMBS PROVEN_CRYPTO_MP_LIMBS(RSA_SIGN_BYTES)
#define RSA_PRIME_LIMBS PROVEN_CRYPTO_MP_CT_MAX

static void rsa_wipe(void *p, proven_size_t n) { proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)p, .size = n }); }

/* Copy an unsigned integer right-aligned into `width` bytes. False when it does not fit. */
static bool rsa_copy_uint(proven_byte_t *out, proven_size_t width, proven_mem_view_t v) {
    while (v.size > 0 && v.ptr[0] == 0) { v.ptr++; v.size--; }
    if (v.size > width) return false;
    for (proven_size_t i = 0; i < width - v.size; ++i) out[i] = 0;
    for (proven_size_t i = 0; i < v.size; ++i) out[width - v.size + i] = v.ptr[i];
    return true;
}

static bool rsa_key_prepare(proven_crypto_rsa_key_t *k);

bool proven_crypto_rsa_key_parse(proven_mem_view_t der, proven_crypto_rsa_key_t *k) {
    if (!k || !der.ptr) return false;
    proven_der_t in = { .p = der.ptr, .n = der.size }, seq;
    proven_u32 version = 1;
    proven_mem_view_t n, e, d, p, q, dp, dq, qinv;
    bool ok = der_expect(&in, DER_SEQUENCE, &seq, (void *)0) && in.n == 0 && der_small_uint(&seq, &version) && version == 0 &&
              der_uint(&seq, &n) && der_uint(&seq, &e) && der_uint(&seq, &d) && der_uint(&seq, &p) && der_uint(&seq, &q) &&
              der_uint(&seq, &dp) && der_uint(&seq, &dq) && der_uint(&seq, &qinv) && seq.n == 0;
    if (!ok) return false;
    proven_mem_view_t *all[4] = { &n, &e, &p, &q };
    for (int i = 0; i < 4; ++i) while (all[i]->size > 0 && all[i]->ptr[0] == 0) { all[i]->ptr++; all[i]->size--; }
    if (n.size * 8 < g_rsa_min_bits || n.size > RSA_SIGN_BYTES || e.size == 0 || e.size > 8) return false;
    if (p.size == 0 || q.size == 0 || p.size > RSA_PRIME_BYTES || q.size > RSA_PRIME_BYTES) return false;
    if ((n.ptr[n.size - 1] & 1u) == 0 || (e.ptr[e.size - 1] & 1u) == 0 || (p.ptr[p.size - 1] & 1u) == 0 || (q.ptr[q.size - 1] & 1u) == 0) return false;
    if (e.size == 1 && e.ptr[0] < 3) return false;
    for (proven_size_t i = 0; i < sizeof *k; ++i) ((proven_byte_t *)k)[i] = 0;
    k->n_len = n.size; k->e_len = e.size; k->p_len = p.size; k->q_len = q.size;
    for (proven_size_t i = 0; i < n.size; ++i) k->n[i] = n.ptr[i];
    for (proven_size_t i = 0; i < e.size; ++i) k->e[i] = e.ptr[i];
    for (proven_size_t i = 0; i < p.size; ++i) k->p[i] = p.ptr[i];
    for (proven_size_t i = 0; i < q.size; ++i) k->q[i] = q.ptr[i];
    ok = rsa_copy_uint(k->dp, p.size, dp) && rsa_copy_uint(k->dq, q.size, dq) && rsa_copy_uint(k->qinv, p.size, qinv);
    /* n must be the product of the two primes given: a key whose parts do not belong together
     * signs wrongly at best. */
    proven_crypto_limb_t pl[RSA_PRIME_LIMBS], ql[RSA_PRIME_LIMBS], prod[2 * RSA_PRIME_LIMBS], nl[2 * RSA_PRIME_LIMBS];
    proven_size_t sp = PROVEN_CRYPTO_MP_LIMBS(p.size), sq = PROVEN_CRYPTO_MP_LIMBS(q.size);
    ok = ok && proven_crypto_mp_load_be(pl, sp, k->p, p.size) && proven_crypto_mp_load_be(ql, sq, k->q, q.size) &&
         proven_crypto_mp_load_be(nl, sp + sq, k->n, n.size);
    if (ok) {
        proven_crypto_mp_mul(prod, pl, sp, ql, sq);
        ok = proven_crypto_mp_eq(prod, nl, sp + sq) != 0;
    }
    rsa_wipe(pl, sizeof pl); rsa_wipe(ql, sizeof ql); rsa_wipe(prod, sizeof prod);
    ok = ok && rsa_key_prepare(k);
    if (!ok) rsa_wipe(k, sizeof *k);
    return ok;
}

/* The moduli of one key, prepared: the key's own, made once (rsa_key_prepare), or - for a
 * key that has none - made here, about 6 KiB on the stack of the one operation. */
typedef struct {
    const proven_crypto_mp_mod_t *n, *p, *q;
    proven_crypto_mp_mod_t own[3];
} proven_rsa_mods_t;

static bool rsa_mods_make(const proven_crypto_rsa_key_t *k, proven_crypto_mp_mod_t *n, proven_crypto_mp_mod_t *p, proven_crypto_mp_mod_t *q) {
    return proven_crypto_mp_mod_init(n, k->n, k->n_len) && proven_crypto_mp_mod_init_secret(p, k->p, k->p_len) &&
           proven_crypto_mp_mod_init_secret(q, k->q, k->q_len) && p->limbs <= RSA_PRIME_LIMBS && q->limbs <= RSA_PRIME_LIMBS &&
           n->limbs <= RSA_SIGN_LIMBS && n->limbs <= p->limbs + q->limbs;
}

static bool rsa_mods(const proven_crypto_rsa_key_t *k, proven_rsa_mods_t *m) {
    if (k->prepared) {
        m->n = &k->mod_n; m->p = &k->mod_p; m->q = &k->mod_q;
        return true;
    }
    m->n = &m->own[0]; m->p = &m->own[1]; m->q = &m->own[2];
    return rsa_mods_make(k, &m->own[0], &m->own[1], &m->own[2]);
}

/* A key that was just read or generated: its moduli, kept with it. */
static bool rsa_key_prepare(proven_crypto_rsa_key_t *k) {
    k->prepared = rsa_mods_make(k, &k->mod_n, &k->mod_p, &k->mod_q);
    return k->prepared;
}

/* The number below n that is m1 modulo p and m2 modulo q (Garner). */
static void rsa_garner(const proven_crypto_rsa_key_t *k, const proven_rsa_mods_t *m, const proven_crypto_limb_t *m1, const proven_crypto_limb_t *m2, proven_crypto_limb_t *out) {
    const proven_size_t sp = m->p->limbs, sq = m->q->limbs;
    proven_crypto_limb_t h[RSA_PRIME_LIMBS], t[RSA_PRIME_LIMBS], prod[2 * RSA_PRIME_LIMBS + 1];
    proven_byte_t bytes[RSA_PRIME_BYTES];
    /* m2 is below q, which says nothing about p: reduce it before subtracting. */
    proven_crypto_mp_store_be(bytes, k->q_len, m2, sq);
    proven_crypto_mp_reduce_be(m->p, t, bytes, k->q_len);
    proven_crypto_mp_sub(m->p, h, m1, t);
    /* h = (m1 - m2) * qinv mod p: one factor in Montgomery form makes the product plain. */
    (void)proven_crypto_mp_load_be(t, sp, k->qinv, k->p_len);
    proven_crypto_mp_to_mont(m->p, h, h);
    proven_crypto_mp_montmul(m->p, h, h, t);
    /* m2 + q * h */
    proven_crypto_mp_mul(prod, m->q->n, sq, h, sp);
    prod[sp + sq] = 0;
    proven_crypto_limb_t carry = proven_crypto_mp_add_raw(prod, prod, m2, sq);
    for (proven_size_t i = sq; i < sp + sq; ++i) {
        const proven_crypto_limb_t v = prod[i] + carry;
        carry = (prod[i] & ~v) >> (PROVEN_CRYPTO_MP_LIMB_BITS - 1);      /* it wrapped: from all ones to zero */
        prod[i] = v;
    }
    for (proven_size_t i = 0; i < m->n->limbs; ++i) out[i] = prod[i];
    rsa_wipe(h, sizeof h); rsa_wipe(t, sizeof t); rsa_wipe(prod, sizeof prod); rsa_wipe(bytes, sizeof bytes);
}

/* x^exp modulo one prime, for x given as n_len big-endian bytes: reduce, exponentiate. */
static bool rsa_half(const proven_crypto_mp_mod_t *prime, const proven_byte_t *x, proven_size_t x_len,
                     const proven_byte_t *exp, proven_size_t exp_len, proven_crypto_limb_t *out) {
    proven_crypto_limb_t b[RSA_PRIME_LIMBS], e[RSA_PRIME_LIMBS];
    proven_crypto_mp_reduce_be(prime, b, x, x_len);
    bool ok = proven_crypto_mp_load_be(e, prime->limbs, exp, exp_len);
    proven_crypto_mp_to_mont(prime, b, b);
    ok = ok && proven_crypto_mp_pow_ct(prime, b, b, e);
    proven_crypto_mp_from_mont(prime, out, b);
    rsa_wipe(b, sizeof b); rsa_wipe(e, sizeof e);
    return ok;
}

/* c^d mod n for c of n_len bytes below n, into `sig`. False when the result does not verify. */
static bool rsa_private(const proven_crypto_rsa_key_t *k, const proven_crypto_rsa_blind_t *blind, const proven_byte_t *c, proven_byte_t *sig) {
    proven_rsa_mods_t m;
    proven_crypto_limb_t cl[RSA_SIGN_LIMBS], x[RSA_SIGN_LIMBS], f[RSA_SIGN_LIMBS], m1[RSA_PRIME_LIMBS], m2[RSA_PRIME_LIMBS], ex[RSA_E_LIMBS];
    proven_byte_t xb[RSA_SIGN_BYTES];
    bool ok = rsa_mods(k, &m) && proven_crypto_mp_load_be(cl, m.n->limbs, c, k->n_len) && proven_crypto_mp_load_be(ex, RSA_E_LIMBS, k->e, k->e_len);
    if (ok) {
        proven_crypto_limb_t below = proven_crypto_mp_lt(cl, m.n->n, m.n->limbs);
        PROVEN_CT_PUBLIC(&below, sizeof below);        /* whether the encoded message is below n: it is, by construction */
        ok = below != 0;
    }
    if (ok) {
        const proven_size_t sn = m.n->limbs;
        for (proven_size_t i = 0; i < sn; ++i) x[i] = cl[i];
        if (blind) {
            (void)proven_crypto_mp_load_be(f, sn, blind->factor, k->n_len);
            proven_crypto_mp_to_mont(m.n, x, x);
            proven_crypto_mp_montmul(m.n, x, x, f);
        }
        proven_crypto_mp_store_be(xb, k->n_len, x, sn);
        ok = rsa_half(m.p, xb, k->n_len, k->dp, k->p_len, m1) && rsa_half(m.q, xb, k->n_len, k->dq, k->q_len, m2);
        if (ok) {
            rsa_garner(k, &m, m1, m2, x);
            if (blind) {
                (void)proven_crypto_mp_load_be(f, sn, blind->unfactor, k->n_len);
                proven_crypto_mp_to_mont(m.n, x, x);
                proven_crypto_mp_montmul(m.n, x, x, f);
            }
            /* The check: x^e must be the message again. */
            proven_crypto_mp_to_mont(m.n, f, x);
            proven_crypto_mp_pow(m.n, f, f, ex, RSA_E_LIMBS);
            proven_crypto_mp_from_mont(m.n, f, f);
            proven_crypto_limb_t same = proven_crypto_mp_eq(f, cl, sn);
            PROVEN_CT_PUBLIC(&same, sizeof same);      /* a signature either verifies or is withheld */
            ok = same != 0;
            if (ok) {
                proven_crypto_mp_store_be(sig, k->n_len, x, sn);
                PROVEN_CT_PUBLIC(sig, k->n_len);                  /* a signature is published */
            }
        }
    }
    rsa_wipe(&m, sizeof m); rsa_wipe(x, sizeof x); rsa_wipe(f, sizeof f); rsa_wipe(m1, sizeof m1); rsa_wipe(m2, sizeof m2); rsa_wipe(xb, sizeof xb);
    return ok;
}

bool proven_crypto_rsa_blind_make(const proven_crypto_rsa_key_t *k, const proven_byte_t *random, proven_crypto_rsa_blind_t *out) {
    proven_rsa_mods_t m;
    proven_crypto_limb_t r[RSA_SIGN_LIMBS], t[RSA_SIGN_LIMBS], ip[RSA_PRIME_LIMBS], iq[RSA_PRIME_LIMBS], e2[RSA_PRIME_LIMBS], two[RSA_PRIME_LIMBS], ex[RSA_E_LIMBS];
    proven_byte_t rb[RSA_SIGN_BYTES], eb[RSA_PRIME_BYTES];
    if (!k || !random || !out) return false;
    bool ok = rsa_mods(k, &m) && proven_crypto_mp_load_be(ex, RSA_E_LIMBS, k->e, k->e_len);
    if (ok) {
        const proven_size_t sn = m.n->limbs;
        proven_crypto_mp_reduce_be(m.n, r, random, k->n_len);
        proven_crypto_mp_store_be(rb, k->n_len, r, sn);
        /* r^e: the exponent is public, the base is not - and the arithmetic does not branch on it. */
        proven_crypto_mp_to_mont(m.n, t, r);
        proven_crypto_mp_pow(m.n, t, t, ex, RSA_E_LIMBS);
        proven_crypto_mp_from_mont(m.n, t, t);
        proven_crypto_mp_store_be(out->factor, k->n_len, t, sn);
        /* r^-1 modulo each prime by Fermat - with the constant-time exponentiation, because
         * here the exponent is the prime less two - and the two recombined. */
        for (int half = 0; half < 2 && ok; ++half) {
            const proven_crypto_mp_mod_t *prime = half == 0 ? m.p : m.q;
            proven_size_t len = half == 0 ? k->p_len : k->q_len;
            /* prime - 2, as bytes for rsa_half. */
            for (proven_size_t i = 0; i < prime->limbs; ++i) two[i] = 0;
            two[0] = 2;
            (void)proven_crypto_mp_sub_raw(e2, prime->n, two, prime->limbs);
            proven_crypto_mp_store_be(eb, len, e2, prime->limbs);
            ok = rsa_half(prime, rb, k->n_len, eb, len, half == 0 ? ip : iq);
        }
        if (ok) {
            rsa_garner(k, &m, ip, iq, t);
            proven_crypto_mp_store_be(out->unfactor, k->n_len, t, sn);
            /* r * r^-1 must be 1: it is not when r shares a factor with n, which random bytes
             * do with negligible probability - and then these bytes are simply not used. */
            proven_crypto_mp_to_mont(m.n, r, r);
            proven_crypto_mp_montmul(m.n, r, r, t);
            for (proven_size_t i = 0; i < sn; ++i) t[i] = 0;
            t[0] = 1;
            proven_crypto_limb_t one = proven_crypto_mp_eq(r, t, sn);
            PROVEN_CT_PUBLIC(&one, sizeof one);
            ok = one != 0;
        }
    }
    rsa_wipe(&m, sizeof m); rsa_wipe(r, sizeof r); rsa_wipe(t, sizeof t); rsa_wipe(ip, sizeof ip); rsa_wipe(iq, sizeof iq);
    rsa_wipe(e2, sizeof e2); rsa_wipe(rb, sizeof rb); rsa_wipe(eb, sizeof eb);
    if (!ok) rsa_wipe(out, sizeof *out);
    return ok;
}

void proven_crypto_rsa_blind_next(const proven_crypto_rsa_key_t *k, proven_crypto_rsa_blind_t *blind) {
    proven_crypto_mp_mod_t own;
    proven_crypto_limb_t v[RSA_SIGN_LIMBS], w[RSA_SIGN_LIMBS];
    if (!k || !blind) return;
    const proven_crypto_mp_mod_t *n = &k->mod_n;
    if (!k->prepared) {
        if (!proven_crypto_mp_mod_init(&own, k->n, k->n_len)) return;
        n = &own;
    }
    if (n->limbs > RSA_SIGN_LIMBS) return;
    for (int half = 0; half < 2; ++half) {
        proven_byte_t *bytes = half == 0 ? blind->factor : blind->unfactor;
        (void)proven_crypto_mp_load_be(v, n->limbs, bytes, k->n_len);
        proven_crypto_mp_to_mont(n, w, v);
        proven_crypto_mp_montmul(n, v, w, v);
        proven_crypto_mp_store_be(bytes, k->n_len, v, n->limbs);
    }
    rsa_wipe(v, sizeof v); rsa_wipe(w, sizeof w);
}

bool proven_crypto_rsa_sign_pkcs1(const proven_crypto_rsa_key_t *k, const proven_crypto_rsa_blind_t *blind, int hash,
                                  proven_mem_view_t digest, proven_byte_t *sig) {
    proven_byte_t em[RSA_SIGN_BYTES];
    const proven_byte_t *di;
    proven_size_t hlen = proven_hmac_size((proven_hmac_hash_t)hash);
    if (!k || !sig || !digest.ptr || hlen == 0 || digest.size != hlen) return false;
    proven_size_t di_len = 0;
    if (!(di = rsa_digest_info(hash, &di_len))) return false;
    const proven_size_t t_len = di_len + hlen, len = k->n_len;
    if (len > RSA_SIGN_BYTES || len < t_len + 11) return false;
    /* 00 01 FF..FF 00 DigestInfo digest */
    proven_size_t pad_end = len - t_len - 1;
    em[0] = 0; em[1] = 1;
    for (proven_size_t i = 2; i < pad_end; ++i) em[i] = 0xff;
    em[pad_end] = 0;
    for (proven_size_t i = 0; i < di_len; ++i) em[pad_end + 1 + i] = di[i];
    for (proven_size_t i = 0; i < hlen; ++i) em[pad_end + 1 + di_len + i] = digest.ptr[i];
    return rsa_private(k, blind, em, sig);
}

bool proven_crypto_rsa_sign_pss(const proven_crypto_rsa_key_t *k, const proven_crypto_rsa_blind_t *blind, int hash,
                                proven_mem_view_t digest, proven_mem_view_t salt, proven_byte_t *sig) {
    proven_byte_t em_full[RSA_SIGN_BYTES], h[PROVEN_HMAC_MAX_SIZE], block[PROVEN_HMAC_MAX_SIZE];
    proven_size_t hlen = proven_hmac_size((proven_hmac_hash_t)hash);
    if (!k || !sig || !digest.ptr || hlen == 0 || digest.size != hlen || (salt.size > 0 && !salt.ptr)) return false;
    const proven_size_t len = k->n_len;
    if (len > RSA_SIGN_BYTES || len == 0) return false;
    /* The encoding is one bit shorter than the modulus. */
    proven_size_t bits = len * 8;
    for (proven_byte_t top = k->n[0]; (top & 0x80u) == 0; top = (proven_byte_t)(top << 1)) bits--;
    const proven_size_t em_bits = bits - 1, em_len = (em_bits + 7) / 8;
    if (em_len < hlen + salt.size + 2) return false;
    proven_byte_t *em = em_full + (len - em_len);
    for (proven_size_t i = 0; i < len - em_len; ++i) em_full[i] = 0;
    static const proven_byte_t zeros[8] = { 0 };
    rsa_hash(hash, (proven_mem_view_t){ .ptr = zeros, .size = 8 }, digest, salt, h);
    /* DB = PS (zeros) | 01 | salt, then masked with MGF1(H). */
    const proven_size_t db_len = em_len - hlen - 1, ps_len = db_len - salt.size - 1;
    for (proven_size_t i = 0; i < ps_len; ++i) em[i] = 0;
    em[ps_len] = 1;
    for (proven_size_t i = 0; i < salt.size; ++i) em[ps_len + 1 + i] = salt.ptr[i];
    for (proven_size_t done = 0, counter = 0; done < db_len; ++counter) {
        proven_byte_t c[4] = { (proven_byte_t)(counter >> 24), (proven_byte_t)(counter >> 16), (proven_byte_t)(counter >> 8), (proven_byte_t)counter };
        rsa_hash(hash, (proven_mem_view_t){ .ptr = h, .size = hlen }, (proven_mem_view_t){ .ptr = c, .size = 4 }, (proven_mem_view_t){ .ptr = c, .size = 0 }, block);
        for (proven_size_t i = 0; i < hlen && done < db_len; ++i, ++done) em[done] ^= block[i];
    }
    em[0] &= (proven_byte_t)(0xffu >> (8 * em_len - em_bits));
    for (proven_size_t i = 0; i < hlen; ++i) em[db_len + i] = h[i];
    em[em_len - 1] = 0xbc;
    return rsa_private(k, blind, em_full, sig);
}

// -----------------------------------------------------------------------------
// Making a key
// -----------------------------------------------------------------------------

/* a mod d for a small d. */
/* ---- Signatures without a DigestInfo: what TLS 1.0 and 1.1 sign with an RSA key is the
 * 36 bytes of an MD5 and a SHA-1 hash, padded as type 1 and nothing more (RFC 2246 7.4.3). ---- */

bool proven_crypto_rsa_sign_pkcs1_raw(const proven_crypto_rsa_key_t *k, const proven_crypto_rsa_blind_t *blind, proven_mem_view_t data, proven_byte_t *sig) {
    proven_byte_t em[RSA_SIGN_BYTES];
    if (!k || !sig || !data.ptr || data.size == 0) return false;
    const proven_size_t len = k->n_len;
    if (len > RSA_SIGN_BYTES || len < data.size + 11) return false;
    const proven_size_t pad_end = len - data.size - 1;
    em[0] = 0; em[1] = 1;
    for (proven_size_t i = 2; i < pad_end; ++i) em[i] = 0xff;
    em[pad_end] = 0;
    for (proven_size_t i = 0; i < data.size; ++i) em[pad_end + 1 + i] = data.ptr[i];
    return rsa_private(k, blind, em, sig);
}

bool proven_crypto_rsa_verify_pkcs1_raw(proven_mem_view_t n, proven_mem_view_t e, proven_mem_view_t data, proven_mem_view_t sig) {
    proven_byte_t em[RSA_MAX_BYTES];
    proven_size_t bits = 0;
    if (!data.ptr || data.size == 0) return false;
    const proven_size_t k = rsa_public(n, e, sig, em, &bits);
    if (k == 0 || k < data.size + 11) return false;
    /* Rebuilt and compared whole, as the other verifier does. */
    proven_byte_t diff = 0;
    const proven_size_t pad_end = k - data.size - 1;
    diff |= em[0];
    diff |= (proven_byte_t)(em[1] ^ 0x01);
    for (proven_size_t i = 2; i < pad_end; ++i) diff |= (proven_byte_t)(em[i] ^ 0xff);
    diff |= em[pad_end];
    for (proven_size_t i = 0; i < data.size; ++i) diff |= (proven_byte_t)(em[pad_end + 1 + i] ^ data.ptr[i]);
    return diff == 0;
}

/* ---- Key exchange by encryption (RFC 5246 section 7.4.7.1), for the legacy TLS suites ---- */

bool proven_crypto_rsa_encrypt_pkcs1(proven_mem_view_t n, proven_mem_view_t e, proven_mem_view_t msg, const proven_byte_t *random,
                                     proven_byte_t *out, proven_size_t *out_len) {
    proven_byte_t em[RSA_MAX_BYTES];
    proven_size_t bits = 0;
    if (!n.ptr || !msg.ptr || !random || !out || !out_len) return false;
    while (n.size > 0 && n.ptr[0] == 0) { n.ptr++; n.size--; }
    const proven_size_t k = n.size;
    if (k > RSA_MAX_BYTES || k < msg.size + 11) return false;
    /* 00 02 | at least eight bytes, none of them zero | 00 | the message */
    const proven_size_t ps = k - 3 - msg.size;
    em[0] = 0; em[1] = 2;
    for (proven_size_t i = 0; i < ps; ++i) em[2 + i] = random[i] ? random[i] : (proven_byte_t)(0x80 | (i & 0x7f) | 1);
    em[2 + ps] = 0;
    for (proven_size_t i = 0; i < msg.size; ++i) em[3 + ps + i] = msg.ptr[i];
    const proven_size_t made = rsa_public(n, e, (proven_mem_view_t){ .ptr = em, .size = k }, out, &bits);
    rsa_wipe(em, sizeof em);
    *out_len = made;
    return made == k;
}

/* The k with 1 + k * f = 0 mod e, for f = (the modulus of the inverse) mod e, f not zero, e prime. */
static proven_u32 rsa_small_k(proven_u32 f, proven_u32 e) {
    /* f^-1 mod e by Fermat: f^(e-2). */
    proven_u64 inv = 1, base = f % e;
    for (proven_u32 x = e - 2; x; x >>= 1) {
        if (x & 1u) inv = inv * base % e;
        base = base * base % e;
    }
    return (proven_u32)((e - inv) % e);
}

/* Is the odd number `x` (limbs limbs, top bit set) prime? Trial division, then Miller-Rabin. */
static bool rsa_is_prime(const proven_byte_t *bytes, proven_size_t len, int rounds, proven_crypto_random_fn random, void *ctx) {
    proven_crypto_mp_mod_t mod;
    proven_crypto_limb_t t[RSA_PRIME_LIMBS], y[RSA_PRIME_LIMBS], one[RSA_PRIME_LIMBS], minus[RSA_PRIME_LIMBS];
    proven_byte_t rnd[RSA_PRIME_BYTES];
    if (!proven_crypto_mp_mod_init(&mod, bytes, len) || mod.limbs > RSA_PRIME_LIMBS) return false;
    const proven_size_t s = mod.limbs;
    /* Small factors first: most candidates end here, at almost no cost. */
    for (proven_u32 d = 3; d < 2000; d += 2) {
        bool d_prime = true;
        for (proven_u32 f = 3; f * f <= d; f += 2) if (d % f == 0) { d_prime = false; break; }
        if (d_prime && proven_crypto_mp_mod_small(mod.n, s, d) == 0) return false;
    }
    /* x - 1 = 2^r * t with t odd. */
    proven_size_t r = 0;
    for (proven_size_t i = 0; i < s; ++i) t[i] = mod.n[i];
    t[0] &= ~(proven_crypto_limb_t)1;
    while ((t[0] & 1u) == 0) {
        for (proven_size_t i = 0; i < s; ++i) t[i] = (t[i] >> 1) | (i + 1 < s ? t[i + 1] << (PROVEN_CRYPTO_MP_LIMB_BITS - 1) : 0u);
        r++;
    }
    for (proven_size_t i = 0; i < s; ++i) one[i] = 0;
    one[0] = 1;
    proven_crypto_mp_to_mont(&mod, one, one);                        /* 1, in Montgomery form */
    for (proven_size_t i = 0; i < s; ++i) y[i] = 0;
    proven_crypto_mp_sub(&mod, minus, y, one);                       /* -1, as 0 - 1 */
    for (int round = 0; round < rounds; ++round) {
        random(ctx, rnd, len);
        proven_crypto_mp_reduce_be(&mod, y, rnd, len);
        proven_crypto_mp_to_mont(&mod, y, y);
        if (proven_crypto_mp_is_zero(y, s) || proven_crypto_mp_eq(y, one, s) || proven_crypto_mp_eq(y, minus, s)) continue;
        if (!proven_crypto_mp_pow_ct(&mod, y, y, t)) return false;
        if (proven_crypto_mp_eq(y, one, s) || proven_crypto_mp_eq(y, minus, s)) continue;
        bool witness = true;
        for (proven_size_t i = 1; i < r && witness; ++i) {
            proven_crypto_mp_montsqr(&mod, y, y);
            if (proven_crypto_mp_eq(y, minus, s)) witness = false;
        }
        if (witness) return false;
    }
    return true;
}

bool proven_crypto_rsa_generate(proven_size_t bits, proven_crypto_random_fn random, void *ctx, proven_crypto_rsa_key_t *k, proven_byte_t *d) {
    const proven_u32 e = 65537;
    if (!random || !k || !d || bits < 1024 || bits > PROVEN_CRYPTO_RSA_SIGN_MAX_BITS || bits % 64 != 0) return false;
    const proven_size_t half = bits / 16, sp = PROVEN_CRYPTO_MP_LIMBS(half);     /* bytes and limbs of one prime */
    const int rounds = half * 8 >= 512 ? 5 : 8;
    proven_crypto_limb_t pl[2][RSA_PRIME_LIMBS], phi[2 * RSA_PRIME_LIMBS], tmp[2 * RSA_PRIME_LIMBS + 1], nl[2 * RSA_PRIME_LIMBS];
    for (proven_size_t i = 0; i < sizeof *k; ++i) ((proven_byte_t *)k)[i] = 0;
    k->n_len = bits / 8; k->e_len = 3; k->p_len = half; k->q_len = half;
    k->e[0] = 1; k->e[1] = 0; k->e[2] = 1;
    for (int which = 0; which < 2; ++which) {
        proven_byte_t *prime = which == 0 ? k->p : k->q;
        for (;;) {
            random(ctx, prime, half);
            prime[0] |= 0xc0;                                        /* two top bits: the product has all its bits */
            prime[half - 1] |= 1;
            (void)proven_crypto_mp_load_be(pl[which], sp, prime, half);
            if (proven_crypto_mp_mod_small(pl[which], sp, e) == 1) continue;      /* e must not divide prime - 1 */
            if (which == 1 && proven_crypto_mp_eq(pl[0], pl[1], sp)) continue;
            if (rsa_is_prime(prime, half, rounds, random, ctx)) break;
        }
    }
    /* n, and the exponents by exact arithmetic: for a prime e, e^-1 mod m = (1 + k m) / e
     * with k = -(m^-1) mod e. */
    proven_crypto_mp_mul(nl, pl[0], sp, pl[1], sp);
    proven_crypto_mp_store_be(k->n, k->n_len, nl, 2 * sp);
    pl[0][0] -= 1; pl[1][0] -= 1;                                    /* p - 1 and q - 1: the primes are odd */
    proven_crypto_mp_inverse_from(tmp, pl[0], sp, rsa_small_k(proven_crypto_mp_mod_small(pl[0], sp, e), e), e);
    proven_crypto_mp_store_be(k->dp, half, tmp, sp);
    proven_crypto_mp_inverse_from(tmp, pl[1], sp, rsa_small_k(proven_crypto_mp_mod_small(pl[1], sp, e), e), e);
    proven_crypto_mp_store_be(k->dq, half, tmp, sp);
    proven_crypto_mp_mul(phi, pl[0], sp, pl[1], sp);
    proven_crypto_mp_inverse_from(tmp, phi, 2 * sp, rsa_small_k(proven_crypto_mp_mod_small(phi, 2 * sp, e), e), e);
    proven_crypto_mp_store_be(d, k->n_len, tmp, 2 * sp);
    /* qinv = q^(p-2) mod p. */
    proven_crypto_mp_mod_t modp;
    proven_byte_t eb[RSA_PRIME_BYTES];
    proven_crypto_limb_t qi[RSA_PRIME_LIMBS];
    bool ok = proven_crypto_mp_mod_init(&modp, k->p, half);
    if (ok) {
        pl[0][0] -= 1;                                               /* p - 2 */
        proven_crypto_mp_store_be(eb, half, pl[0], sp);
        ok = rsa_half(&modp, k->q, half, eb, half, qi);
        proven_crypto_mp_store_be(k->qinv, half, qi, sp);
    }
    rsa_wipe(pl, sizeof pl); rsa_wipe(phi, sizeof phi); rsa_wipe(tmp, sizeof tmp); rsa_wipe(eb, sizeof eb); rsa_wipe(qi, sizeof qi); rsa_wipe(&modp, sizeof modp);
    ok = ok && rsa_key_prepare(k);
    if (!ok) rsa_wipe(k, sizeof *k);
    return ok;
}
