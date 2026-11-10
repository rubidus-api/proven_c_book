#include "proven_sys_aes.h"

/* Hardware AES and carry-less multiplication. Only the instruction sequences live here; the
 * mode, the key schedule and the choice between this and the portable code are in
 * src/proven/crypto_aes.c. */

#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__)) && !defined(PROVEN_SYS_AES_DISABLE)

/* gcc at -O2 leaves the loops over the blocks in flight rolled: the blocks then live in memory and
 * this code runs at about half its speed (2.2 cycles a byte against 1.2, RFC-0023). Where the
 * build optimises at all, gcc optimises this part as at -O3 (clang unrolls them at -O2). The
 * pragma comes before the intrinsics' header: functions with other optimisation options than
 * the always-inline intrinsics they call are refused at -O1 and -Os ("target specific option
 * mismatch"). At -Os this part is larger than the build asked for. */
#if !defined(__clang__) && defined(__OPTIMIZE__)
#pragma GCC optimize("O3")
#endif

#include <immintrin.h>

#define AES_TARGET __attribute__((target("aes,pclmul,ssse3,sse4.1")))

bool proven_sys_aes_available(void) {
    __builtin_cpu_init();
    return __builtin_cpu_supports("aes") && __builtin_cpu_supports("pclmul") &&
           __builtin_cpu_supports("ssse3") && __builtin_cpu_supports("sse4.1");
}

AES_TARGET static __m128i aes_block(const uint8_t *rk, int rounds, __m128i b) {
    b = _mm_xor_si128(b, _mm_loadu_si128((const __m128i *)rk));
    for (int r = 1; r < rounds; ++r) b = _mm_aesenc_si128(b, _mm_loadu_si128((const __m128i *)(rk + 16 * r)));
    return _mm_aesenclast_si128(b, _mm_loadu_si128((const __m128i *)(rk + 16 * rounds)));
}

AES_TARGET void proven_sys_aes_encrypt_block(const uint8_t *rk, int rounds, const uint8_t in[16], uint8_t out[16]) {
    _mm_storeu_si128((__m128i *)out, aes_block(rk, rounds, _mm_loadu_si128((const __m128i *)in)));
}

/* The round keys, loaded once for a call that has many blocks to do. */
typedef struct { __m128i k[15]; } proven_aes_keys_t;

AES_TARGET static void aes_keys_load(proven_aes_keys_t *ks, const uint8_t *rk, int rounds) {
    for (int r = 0; r <= rounds; ++r) ks->k[r] = _mm_loadu_si128((const __m128i *)(rk + 16 * r));
}

/* Zero what held key material, with the barrier proven_mem_wipe uses: the compiler must
 * assume the empty statement reads it. */
AES_TARGET static void aes_forget(void *p, size_t n) {
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; ++i) b[i] = 0;
    __asm__ __volatile__("" : : "r"(p) : "memory");
}

/* The key schedule with AESKEYGENASSIST, as in Intel's AES-NI white paper (section 5.2): the
 * instruction gives SubWord and RotWord of the last word with the round constant, and the
 * shifts and XORs spread it over the four words of the next round key. Its constant must be an
 * immediate, so the rounds are written out. */
AES_TARGET static __m128i key_step(__m128i k, __m128i assist) {
    assist = _mm_shuffle_epi32(assist, 0xff);
    __m128i t = _mm_slli_si128(k, 4);
    k = _mm_xor_si128(k, t); t = _mm_slli_si128(t, 4);
    k = _mm_xor_si128(k, t); t = _mm_slli_si128(t, 4);
    k = _mm_xor_si128(k, t);
    return _mm_xor_si128(k, assist);
}

/* AES-256's odd round keys: SubWord of the last word, no rotation and no constant. */
AES_TARGET static __m128i key_step_odd(__m128i prev_even, __m128i k) {
    __m128i assist = _mm_shuffle_epi32(_mm_aeskeygenassist_si128(prev_even, 0x00), 0xaa);
    __m128i t = _mm_slli_si128(k, 4);
    k = _mm_xor_si128(k, t); t = _mm_slli_si128(t, 4);
    k = _mm_xor_si128(k, t); t = _mm_slli_si128(t, 4);
    k = _mm_xor_si128(k, t);
    return _mm_xor_si128(k, assist);
}

#define KEY128(i, rcon) do { k = key_step(k, _mm_aeskeygenassist_si128(k, rcon)); _mm_storeu_si128((__m128i *)(rk + 16 * (i)), k); } while (0)
#define KEY256(i, rcon) do { \
        a = key_step(a, _mm_aeskeygenassist_si128(b, rcon)); _mm_storeu_si128((__m128i *)(rk + 16 * (i)), a); \
        if ((i) < 14) { b = key_step_odd(a, b); _mm_storeu_si128((__m128i *)(rk + 16 * ((i) + 1)), b); } \
    } while (0)

AES_TARGET bool proven_sys_aes_expand(uint8_t *rk, int rounds, const uint8_t *key, size_t key_len) {
    if (key_len == 16 && rounds == 10) {
        __m128i k = _mm_loadu_si128((const __m128i *)key);
        _mm_storeu_si128((__m128i *)rk, k);
        KEY128(1, 0x01); KEY128(2, 0x02); KEY128(3, 0x04); KEY128(4, 0x08); KEY128(5, 0x10);
        KEY128(6, 0x20); KEY128(7, 0x40); KEY128(8, 0x80); KEY128(9, 0x1b); KEY128(10, 0x36);
        aes_forget(&k, sizeof k);
        return true;
    }
    if (key_len == 32 && rounds == 14) {
        __m128i a = _mm_loadu_si128((const __m128i *)key), b = _mm_loadu_si128((const __m128i *)(key + 16));
        _mm_storeu_si128((__m128i *)rk, a);
        _mm_storeu_si128((__m128i *)(rk + 16), b);
        KEY256(2, 0x01); KEY256(4, 0x02); KEY256(6, 0x04); KEY256(8, 0x08); KEY256(10, 0x10); KEY256(12, 0x20); KEY256(14, 0x40);
        aes_forget(&a, sizeof a);
        aes_forget(&b, sizeof b);
        return true;
    }
    return false;
}

#undef KEY128
#undef KEY256

/* Counter blocks in flight, and blocks folded into one reduction: no block waits for another,
 * and a round takes several cycles. Eight where there are sixteen registers to hold them and
 * the round key; four on 32-bit x86, which has eight. (PROVEN_SYS_AES_WIDE is for a test
 * build that wants the four-block code on a 64-bit machine.) */
#if defined(PROVEN_SYS_AES_WIDE)
#define AES_WIDE PROVEN_SYS_AES_WIDE
#elif defined(__x86_64__)
#define AES_WIDE 8
#else
#define AES_WIDE 4
#endif

AES_TARGET void proven_sys_aes_ctr(const uint8_t *rk, int rounds, const uint8_t iv[12], uint32_t counter,
                                   const uint8_t *in, uint8_t *out, size_t len) {
    uint8_t block[16], ks[16];
    for (int i = 0; i < 12; ++i) block[i] = iv[i];
    if (len >= 16 * AES_WIDE) {
        proven_aes_keys_t keys;
        aes_keys_load(&keys, rk, rounds);
        block[12] = block[13] = block[14] = block[15] = 0;
        /* The nonce, zeros where the counter goes, and the first round key already folded in:
         * a block's counter is then XORed into the last four bytes. */
        const __m128i base = _mm_xor_si128(_mm_loadu_si128((const __m128i *)block), keys.k[0]);
        do {
            __m128i b[AES_WIDE];
            for (int i = 0; i < AES_WIDE; ++i) {
                b[i] = _mm_xor_si128(base, _mm_insert_epi32(_mm_setzero_si128(), (int)__builtin_bswap32(counter + (uint32_t)i), 3));
            }
            counter += AES_WIDE;
            for (int r = 1; r < rounds; ++r) {
                for (int i = 0; i < AES_WIDE; ++i) b[i] = _mm_aesenc_si128(b[i], keys.k[r]);
            }
            for (int i = 0; i < AES_WIDE; ++i) {
                b[i] = _mm_aesenclast_si128(b[i], keys.k[rounds]);
                /* Each block's input is read before that block is written: `in` may be `out`. */
                _mm_storeu_si128((__m128i *)(out + 16 * i), _mm_xor_si128(b[i], _mm_loadu_si128((const __m128i *)(in + 16 * i))));
            }
            in += 16 * AES_WIDE; out += 16 * AES_WIDE; len -= 16 * AES_WIDE;
        } while (len >= 16 * AES_WIDE);
        aes_forget(&keys, sizeof keys);
    }
    while (len > 0) {
        block[12] = (uint8_t)(counter >> 24); block[13] = (uint8_t)(counter >> 16);
        block[14] = (uint8_t)(counter >> 8); block[15] = (uint8_t)counter;
        counter++;
        __m128i k = aes_block(rk, rounds, _mm_loadu_si128((const __m128i *)block));
        if (len >= 16) {
            _mm_storeu_si128((__m128i *)out, _mm_xor_si128(k, _mm_loadu_si128((const __m128i *)in)));
            in += 16; out += 16; len -= 16;
        } else {
            _mm_storeu_si128((__m128i *)ks, k);
            for (size_t i = 0; i < len; ++i) out[i] = (uint8_t)(in[i] ^ ks[i]);
            len = 0;
        }
    }
}

/* The multiplication of Intel's carry-less multiplication white paper, in its two halves:
 * operands are byte reversed and multiplied as polynomials into 256 bits (gf_product); the
 * 256 bits are shifted one bit for GCM's reflected order and reduced (gf_reduce). Both
 * halves are linear, so several products may be added before one reduction. */
AES_TARGET static void gf_product(__m128i a, __m128i b, __m128i *lo, __m128i *hi) {
    __m128i t3 = _mm_clmulepi64_si128(a, b, 0x00);
    __m128i t4 = _mm_clmulepi64_si128(a, b, 0x10);
    __m128i t5 = _mm_clmulepi64_si128(a, b, 0x01);
    __m128i t6 = _mm_clmulepi64_si128(a, b, 0x11);
    t4 = _mm_xor_si128(t4, t5);
    *lo = _mm_xor_si128(t3, _mm_slli_si128(t4, 8));
    *hi = _mm_xor_si128(t6, _mm_srli_si128(t4, 8));
}

AES_TARGET static __m128i gf_reduce(__m128i t3, __m128i t6) {
    __m128i t2, t4, t5, t7, t8, t9;
    t7 = _mm_srli_epi32(t3, 31);
    t8 = _mm_srli_epi32(t6, 31);
    t3 = _mm_slli_epi32(t3, 1);
    t6 = _mm_slli_epi32(t6, 1);
    t9 = _mm_srli_si128(t7, 12);
    t8 = _mm_slli_si128(t8, 4);
    t7 = _mm_slli_si128(t7, 4);
    t3 = _mm_or_si128(t3, t7);
    t6 = _mm_or_si128(t6, t8);
    t6 = _mm_or_si128(t6, t9);
    t7 = _mm_slli_epi32(t3, 31);
    t8 = _mm_slli_epi32(t3, 30);
    t9 = _mm_slli_epi32(t3, 25);
    t7 = _mm_xor_si128(t7, t8);
    t7 = _mm_xor_si128(t7, t9);
    t8 = _mm_srli_si128(t7, 4);
    t7 = _mm_slli_si128(t7, 12);
    t3 = _mm_xor_si128(t3, t7);
    t2 = _mm_srli_epi32(t3, 1);
    t4 = _mm_srli_epi32(t3, 2);
    t5 = _mm_srli_epi32(t3, 7);
    t2 = _mm_xor_si128(t2, t4);
    t2 = _mm_xor_si128(t2, t5);
    t2 = _mm_xor_si128(t2, t8);
    t3 = _mm_xor_si128(t3, t2);
    return _mm_xor_si128(t6, t3);
}

AES_TARGET static __m128i gf_mul(__m128i a, __m128i b) {
    __m128i lo, hi;
    gf_product(a, b, &lo, &hi);
    return gf_reduce(lo, hi);
}

/* `count` blocks (1 to AES_WIDE) folded into y with one reduction:
 * y = (y ^ x1)*H^count ^ x2*H^(count-1) ^ ... ^ x_count*H, with hp[i] = H^(i+1). */
AES_TARGET static __m128i gf_fold(__m128i y, const __m128i *hp, const uint8_t *data, int count, __m128i rev) {
    __m128i lo = _mm_setzero_si128(), hi = _mm_setzero_si128();
    for (int i = 0; i < count; ++i) {
        __m128i x = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(data + 16 * i)), rev), l, h;
        if (i == 0) x = _mm_xor_si128(x, y);
        gf_product(x, hp[count - 1 - i], &l, &h);
        lo = _mm_xor_si128(lo, l);
        hi = _mm_xor_si128(hi, h);
    }
    return gf_reduce(lo, hi);
}

AES_TARGET void proven_sys_aes_ghash(uint8_t y[16], const uint8_t h[16], const uint8_t *data, size_t blocks) {
    const __m128i rev = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __m128i hv = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)h), rev);
    __m128i yv = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)y), rev);
    if (blocks >= AES_WIDE) {
        /* The powers of H are made here, for this call. GCM itself goes through
         * proven_sys_aes_gcm, which keeps them with the key (RFC-0023); this call is the
         * two-pass path's, kept for a context made without them. */
        _Static_assert(AES_WIDE == 8 || AES_WIDE == 4, "the powers below are written out for four and for eight");
        __m128i hp[AES_WIDE];
        hp[0] = hv;                                  /* (in an order where no product waits */
        hp[1] = gf_mul(hp[0], hp[0]);                /*  for more than two before it)       */
        hp[2] = gf_mul(hp[1], hp[0]);
        hp[3] = gf_mul(hp[1], hp[1]);
#if AES_WIDE == 8
        hp[4] = gf_mul(hp[3], hp[0]);
        hp[5] = gf_mul(hp[2], hp[2]);
        hp[6] = gf_mul(hp[3], hp[2]);
        hp[7] = gf_mul(hp[3], hp[3]);
#endif
        do {
            yv = gf_fold(yv, hp, data, AES_WIDE, rev);
            data += 16 * AES_WIDE; blocks -= AES_WIDE;
        } while (blocks >= AES_WIDE);
        if (blocks > 0) {
            yv = gf_fold(yv, hp, data, (int)blocks, rev);
            blocks = 0;
        }
        aes_forget(hp, sizeof hp);
    }
    while (blocks-- > 0) {
        __m128i x = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)data), rev);
        yv = gf_mul(_mm_xor_si128(yv, x), hv);
        data += 16;
    }
    _mm_storeu_si128((__m128i *)y, _mm_shuffle_epi8(yv, rev));
}

/* ---- GCM in one pass ---- */

AES_TARGET bool proven_sys_aes_gcm_powers(const uint8_t h[16], uint8_t hpow[112]) {
    const __m128i rev = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __m128i hp[8];
    hp[0] = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)h), rev);
    hp[1] = gf_mul(hp[0], hp[0]);
    hp[2] = gf_mul(hp[1], hp[0]);
    hp[3] = gf_mul(hp[1], hp[1]);
    hp[4] = gf_mul(hp[3], hp[0]);
    hp[5] = gf_mul(hp[2], hp[2]);
    hp[6] = gf_mul(hp[3], hp[2]);
    hp[7] = gf_mul(hp[3], hp[3]);
    for (int i = 1; i < 8; ++i) _mm_storeu_si128((__m128i *)(hpow + 16 * (i - 1)), hp[i]);
    aes_forget(hp, sizeof hp);
    return true;
}

/* GHASH's input as a list of blocks, folded at most eight to a reduction: y is multiplied in
 * with the first of each group. */
AES_TARGET static __m128i gf_fold_list(__m128i y, const __m128i *hp, const uint8_t *const *blk, int n, __m128i rev) {
    for (int at = 0; at < n;) {
        const int count = n - at < 8 ? n - at : 8;
        __m128i lo = _mm_setzero_si128(), hi = _mm_setzero_si128();
        for (int i = 0; i < count; ++i) {
            __m128i x = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)blk[at + i]), rev), l, h;
            if (i == 0) x = _mm_xor_si128(x, y);
            gf_product(x, hp[count - 1 - i], &l, &h);
            lo = _mm_xor_si128(lo, l);
            hi = _mm_xor_si128(hi, h);
        }
        y = gf_reduce(lo, hi);
        at += count;
    }
    return y;
}

/* `len` bytes at `data` into y: whole groups of eight, then the rest with the last block zero
 * padded, and `extra` (the lengths block, or NULL) after them in the same fold. */
AES_TARGET static __m128i gf_absorb(__m128i y, const __m128i *hp, const uint8_t *data, size_t len, const uint8_t *extra, __m128i rev) {
    while (len >= 128) {
        y = gf_fold(y, hp, data, 8, rev);
        data += 128; len -= 128;
    }
    uint8_t pad[16];
    const uint8_t *blk[9];
    int n = 0;
    for (; len >= 16; data += 16, len -= 16) blk[n++] = data;
    if (len > 0) {
        for (size_t i = 0; i < 16; ++i) pad[i] = i < len ? data[i] : 0;
        blk[n++] = pad;
    }
    if (extra) blk[n++] = extra;
    y = gf_fold_list(y, hp, blk, n, rev);
    aes_forget(pad, sizeof pad);
    return y;
}

/* Counter mode over fewer than 16 * AES_WIDE bytes, every block in flight at once. */
AES_TARGET static void ctr_rest(const proven_aes_keys_t *keys, int rounds, __m128i base, uint32_t counter, const uint8_t *in, uint8_t *out, size_t len) {
    __m128i b[AES_WIDE];
    const int n = (int)((len + 15) / 16);
    for (int i = 0; i < n; ++i) b[i] = _mm_xor_si128(base, _mm_insert_epi32(_mm_setzero_si128(), (int)__builtin_bswap32(counter + (uint32_t)i), 3));
    for (int r = 1; r < rounds; ++r) {
        for (int i = 0; i < n; ++i) b[i] = _mm_aesenc_si128(b[i], keys->k[r]);
    }
    for (int i = 0; i < n; ++i) {
        b[i] = _mm_aesenclast_si128(b[i], keys->k[rounds]);
        if (len >= 16) {
            _mm_storeu_si128((__m128i *)out, _mm_xor_si128(b[i], _mm_loadu_si128((const __m128i *)in)));
            in += 16; out += 16; len -= 16;
        } else {
            uint8_t ks[16];
            _mm_storeu_si128((__m128i *)ks, b[i]);
            for (size_t j = 0; j < len; ++j) out[j] = (uint8_t)(in[j] ^ ks[j]);
            aes_forget(ks, sizeof ks);
        }
    }
    aes_forget(b, sizeof b);
}

/* Each group of AES_WIDE counter blocks goes through its rounds while AES_WIDE ciphertext
 * blocks are multiplied, one product a round: the cipher's instructions and the carry-less
 * multiply use different execution units, so the two halves overlap instead of following each
 * other. Sealing hashes the group before (the one just written); opening hashes the group's
 * own ciphertext, read before its plaintext is stored over it. */
AES_TARGET bool proven_sys_aes_gcm(const uint8_t *rk, int rounds, const uint8_t h[16], const uint8_t hpow[112], const uint8_t nonce[12],
                                   const uint8_t *aad, size_t aad_len, const uint8_t *in, uint8_t *out, size_t len, bool decrypt, uint8_t tag[16]) {
    const __m128i rev = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    proven_aes_keys_t keys;
    aes_keys_load(&keys, rk, rounds);
    __m128i hp[8];
    hp[0] = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)h), rev);
    for (int i = 1; i < 8; ++i) hp[i] = _mm_loadu_si128((const __m128i *)(hpow + 16 * (i - 1)));
    uint8_t block[16];
    for (int i = 0; i < 12; ++i) block[i] = nonce[i];
    block[12] = block[13] = block[14] = 0; block[15] = 1;
    /* E(J0), the tag's mask: started first, it runs alongside the work that follows. */
    __m128i ej0 = aes_block(rk, rounds, _mm_loadu_si128((const __m128i *)block));
    block[15] = 0;
    const __m128i base = _mm_xor_si128(_mm_loadu_si128((const __m128i *)block), keys.k[0]);
    __m128i y = _mm_setzero_si128();
    if (aad_len > 0) y = gf_absorb(y, hp, aad, aad_len, NULL, rev);
    uint32_t counter = 2;
    const uint8_t *prev = NULL;                  /* sealing: the group written last, not yet hashed */
    while (len >= 16 * AES_WIDE) {
        const uint8_t *hsrc = decrypt ? in : prev;
        __m128i b[AES_WIDE], lo = _mm_setzero_si128(), hi = _mm_setzero_si128();
        for (int i = 0; i < AES_WIDE; ++i) {
            b[i] = _mm_xor_si128(base, _mm_insert_epi32(_mm_setzero_si128(), (int)__builtin_bswap32(counter + (uint32_t)i), 3));
        }
        counter += AES_WIDE;
        for (int r = 1; r < rounds; ++r) {
            for (int i = 0; i < AES_WIDE; ++i) b[i] = _mm_aesenc_si128(b[i], keys.k[r]);
            if (hsrc && r <= AES_WIDE) {
                __m128i x = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(hsrc + 16 * (r - 1))), rev), l, hh;
                if (r == 1) x = _mm_xor_si128(x, y);
                gf_product(x, hp[AES_WIDE - r], &l, &hh);
                lo = _mm_xor_si128(lo, l);
                hi = _mm_xor_si128(hi, hh);
            }
        }
        if (hsrc) y = gf_reduce(lo, hi);
        for (int i = 0; i < AES_WIDE; ++i) {
            b[i] = _mm_aesenclast_si128(b[i], keys.k[rounds]);
            _mm_storeu_si128((__m128i *)(out + 16 * i), _mm_xor_si128(b[i], _mm_loadu_si128((const __m128i *)(in + 16 * i))));
        }
        prev = out;
        in += 16 * AES_WIDE; out += 16 * AES_WIDE; len -= 16 * AES_WIDE;
    }
    if (!decrypt && prev) y = gf_fold(y, hp, prev, AES_WIDE, rev);
    /* The rest: fewer than AES_WIDE blocks, and the lengths. Opening hashes the ciphertext
     * before the plaintext overwrites it; sealing, after. */
    uint8_t lens[16];
    const uint64_t abits = (uint64_t)aad_len * 8, cbits = ((uint64_t)(counter - 2) * 16 + len) * 8;
    for (int i = 0; i < 8; ++i) { lens[7 - i] = (uint8_t)(abits >> (8 * i)); lens[15 - i] = (uint8_t)(cbits >> (8 * i)); }
    if (decrypt) y = gf_absorb(y, hp, in, len, lens, rev);
    if (len > 0) ctr_rest(&keys, rounds, base, counter, in, out, len);
    if (!decrypt) y = gf_absorb(y, hp, out, len, lens, rev);
    _mm_storeu_si128((__m128i *)tag, _mm_xor_si128(_mm_shuffle_epi8(y, rev), ej0));
    aes_forget(&keys, sizeof keys);
    aes_forget(hp, sizeof hp);
    aes_forget(block, sizeof block);
    return true;
}

/* The inverse cipher in the form the instructions want: the round keys in reverse, the
 * middle ones passed through InvMixColumns. Made per call - nine or thirteen instructions. */
AES_TARGET bool proven_sys_aes_cbc_decrypt(const uint8_t *rk, int rounds, uint8_t iv[16], uint8_t *data, size_t len) {
    __m128i dk[15];
    dk[0] = _mm_loadu_si128((const __m128i *)(rk + 16 * rounds));
    for (int r = 1; r < rounds; ++r) dk[r] = _mm_aesimc_si128(_mm_loadu_si128((const __m128i *)(rk + 16 * (rounds - r))));
    dk[rounds] = _mm_loadu_si128((const __m128i *)rk);
    __m128i prev = _mm_loadu_si128((const __m128i *)iv);
    for (size_t at = 0; at + 16 <= len; at += 16) {
        const __m128i c = _mm_loadu_si128((const __m128i *)(data + at));
        __m128i b = _mm_xor_si128(c, dk[0]);
        for (int r = 1; r < rounds; ++r) b = _mm_aesdec_si128(b, dk[r]);
        b = _mm_aesdeclast_si128(b, dk[rounds]);
        _mm_storeu_si128((__m128i *)(data + at), _mm_xor_si128(b, prev));
        prev = c;
    }
    _mm_storeu_si128((__m128i *)iv, prev);
    for (int r = 0; r <= rounds; ++r) dk[r] = _mm_setzero_si128();
    return true;
}

#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(PROVEN_SYS_AES_DISABLE)

#include <arm_neon.h>

#if defined(__linux__)
#include <sys/auxv.h>
bool proven_sys_aes_available(void) {
    unsigned long caps = getauxval(AT_HWCAP);
    return (caps & (1ul << 3)) != 0 && (caps & (1ul << 4)) != 0;      /* HWCAP_AES, HWCAP_PMULL */
}
#elif defined(__APPLE__)
bool proven_sys_aes_available(void) { return true; }                   /* every Apple AArch64 core has both */
#elif defined(_WIN32)
#include <windows.h>
bool proven_sys_aes_available(void) { return IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE) != 0; }
#else
bool proven_sys_aes_available(void) { return false; }
#endif

#if defined(__clang__)
#define AES_TARGET __attribute__((target("aes")))
#else
#define AES_TARGET __attribute__((target("+aes")))
#endif

AES_TARGET static uint8x16_t aes_block(const uint8_t *rk, int rounds, uint8x16_t b) {
    /* AESE is AddRoundKey, SubBytes, ShiftRows; AESMC is MixColumns. */
    for (int r = 0; r < rounds - 1; ++r) b = vaesmcq_u8(vaeseq_u8(b, vld1q_u8(rk + 16 * r)));
    b = vaeseq_u8(b, vld1q_u8(rk + 16 * (rounds - 1)));
    return veorq_u8(b, vld1q_u8(rk + 16 * rounds));
}

AES_TARGET void proven_sys_aes_encrypt_block(const uint8_t *rk, int rounds, const uint8_t in[16], uint8_t out[16]) {
    vst1q_u8(out, aes_block(rk, rounds, vld1q_u8(in)));
}

AES_TARGET void proven_sys_aes_ctr(const uint8_t *rk, int rounds, const uint8_t iv[12], uint32_t counter,
                                   const uint8_t *in, uint8_t *out, size_t len) {
    uint8_t block[16], ks[16];
    for (int i = 0; i < 12; ++i) block[i] = iv[i];
    while (len > 0) {
        block[12] = (uint8_t)(counter >> 24); block[13] = (uint8_t)(counter >> 16);
        block[14] = (uint8_t)(counter >> 8); block[15] = (uint8_t)counter;
        counter++;
        uint8x16_t k = aes_block(rk, rounds, vld1q_u8(block));
        if (len >= 16) {
            vst1q_u8(out, veorq_u8(k, vld1q_u8(in)));
            in += 16; out += 16; len -= 16;
        } else {
            vst1q_u8(ks, k);
            for (size_t i = 0; i < len; ++i) out[i] = (uint8_t)(in[i] ^ ks[i]);
            len = 0;
        }
    }
}

/* With the bits of every byte reversed, a GCM block is an ordinary little-endian polynomial:
 * multiply 128 x 128 with four 64-bit carry-less products, then fold the upper half down
 * twice with x^128 = x^7 + x^2 + x + 1. */
AES_TARGET static uint8x16_t gf_mul(uint8x16_t a8, uint8x16_t b8) {
    const poly64_t a0 = (poly64_t)vgetq_lane_u64(vreinterpretq_u64_u8(a8), 0), a1 = (poly64_t)vgetq_lane_u64(vreinterpretq_u64_u8(a8), 1);
    const poly64_t b0 = (poly64_t)vgetq_lane_u64(vreinterpretq_u64_u8(b8), 0), b1 = (poly64_t)vgetq_lane_u64(vreinterpretq_u64_u8(b8), 1);
    uint64x2_t lo = vreinterpretq_u64_p128(vmull_p64(a0, b0));
    uint64x2_t hi = vreinterpretq_u64_p128(vmull_p64(a1, b1));
    uint64x2_t mid = veorq_u64(vreinterpretq_u64_p128(vmull_p64(a0, b1)), vreinterpretq_u64_p128(vmull_p64(a1, b0)));
    const uint64x2_t zero = vdupq_n_u64(0);
    lo = veorq_u64(lo, vextq_u64(zero, mid, 1));          /* mid << 64 */
    hi = veorq_u64(hi, vextq_u64(mid, zero, 1));          /* mid >> 64 */
    /* The top 64 bits of hi, times 0x87, land at x^64: low part into lo's upper half, the
     * overflow joins hi's lower half. */
    uint64x2_t t = vreinterpretq_u64_p128(vmull_p64((poly64_t)vgetq_lane_u64(hi, 1), (poly64_t)0x87));
    lo = veorq_u64(lo, vextq_u64(zero, t, 1));
    uint64_t h0 = vgetq_lane_u64(hi, 0) ^ vgetq_lane_u64(t, 1);
    t = vreinterpretq_u64_p128(vmull_p64((poly64_t)h0, (poly64_t)0x87));
    lo = veorq_u64(lo, t);
    return vreinterpretq_u8_u64(lo);
}

AES_TARGET void proven_sys_aes_ghash(uint8_t y[16], const uint8_t h[16], const uint8_t *data, size_t blocks) {
    uint8x16_t hv = vrbitq_u8(vld1q_u8(h));
    uint8x16_t yv = vrbitq_u8(vld1q_u8(y));
    while (blocks-- > 0) {
        yv = gf_mul(veorq_u8(yv, vrbitq_u8(vld1q_u8(data))), hv);
        data += 16;
    }
    vst1q_u8(y, vrbitq_u8(yv));
}

bool proven_sys_aes_expand(uint8_t *rk, int rounds, const uint8_t *key, size_t key_len) {
    (void)rk; (void)rounds; (void)key; (void)key_len;
    return false;                                  /* the portable schedule: no AArch64 code yet */
}

bool proven_sys_aes_cbc_decrypt(const uint8_t *rk, int rounds, uint8_t iv[16], uint8_t *data, size_t len) {
    (void)rk; (void)rounds; (void)iv; (void)data; (void)len;
    return false;
}

bool proven_sys_aes_gcm_powers(const uint8_t h[16], uint8_t hpow[112]) {
    (void)h; (void)hpow;
    return false;
}

bool proven_sys_aes_gcm(const uint8_t *rk, int rounds, const uint8_t h[16], const uint8_t hpow[112], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aad_len, const uint8_t *in, uint8_t *out, size_t len, bool decrypt, uint8_t tag[16]) {
    (void)rk; (void)rounds; (void)h; (void)hpow; (void)nonce; (void)aad; (void)aad_len; (void)in; (void)out; (void)len; (void)decrypt; (void)tag;
    return false;
}

#else

bool proven_sys_aes_available(void) { return false; }

bool proven_sys_aes_expand(uint8_t *rk, int rounds, const uint8_t *key, size_t key_len) {
    (void)rk; (void)rounds; (void)key; (void)key_len;
    return false;
}

bool proven_sys_aes_cbc_decrypt(const uint8_t *rk, int rounds, uint8_t iv[16], uint8_t *data, size_t len) {
    (void)rk; (void)rounds; (void)iv; (void)data; (void)len;
    return false;
}

bool proven_sys_aes_gcm_powers(const uint8_t h[16], uint8_t hpow[112]) {
    (void)h; (void)hpow;
    return false;
}

bool proven_sys_aes_gcm(const uint8_t *rk, int rounds, const uint8_t h[16], const uint8_t hpow[112], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aad_len, const uint8_t *in, uint8_t *out, size_t len, bool decrypt, uint8_t tag[16]) {
    (void)rk; (void)rounds; (void)h; (void)hpow; (void)nonce; (void)aad; (void)aad_len; (void)in; (void)out; (void)len; (void)decrypt; (void)tag;
    return false;
}

void proven_sys_aes_encrypt_block(const uint8_t *rk, int rounds, const uint8_t in[16], uint8_t out[16]) {
    (void)rk; (void)rounds; (void)in; (void)out;
}

void proven_sys_aes_ctr(const uint8_t *rk, int rounds, const uint8_t iv[12], uint32_t counter,
                        const uint8_t *in, uint8_t *out, size_t len) {
    (void)rk; (void)rounds; (void)iv; (void)counter; (void)in; (void)out; (void)len;
}

void proven_sys_aes_ghash(uint8_t y[16], const uint8_t h[16], const uint8_t *data, size_t blocks) {
    (void)y; (void)h; (void)data; (void)blocks;
}

#endif
