#include "proven/hash.h"

/*
 * Four hashes, one per use case, each implemented from its specification and checked in
 * tests/test_unit_hash against that specification's own known-answer vectors. Written to
 * the contract in include/proven/hash.h, whose test landed red one commit earlier.
 *
 * Everything here is endianness-independent by construction: bytes go in and come out in a
 * fixed order regardless of the host, because a fingerprint that changed with the machine
 * would not be a fingerprint of the content.
 */

// =============================================================================
// FNV-1a 64 - fast table hash, trusted input
// =============================================================================

proven_u64 proven_hash_bytes(proven_mem_view_t data) {
    /* 64-bit offset basis and prime. xor the byte in, THEN multiply (that ordering is what
     * makes it FNV-1a rather than FNV-1, and it avalanches better). */
    proven_u64 h = 0xcbf29ce484222325ull;
    if (data.size > 0 && !data.ptr) return h;   /* a size>0 view with no pointer: consistent with the SHA path */
    for (proven_size_t i = 0; i < data.size; ++i) {
        h ^= (proven_u64)data.ptr[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

// =============================================================================
// SipHash-2-4 - keyed table hash, untrusted input
// =============================================================================

static proven_u64 sip_rotl(proven_u64 x, int b) {
    return (x << b) | (x >> (64 - b));
}

static proven_u64 sip_read_le64(const proven_byte_t *p) {
    proven_u64 v = 0;
    for (int i = 0; i < 8; ++i) v |= (proven_u64)p[i] << (8 * i);
    return v;
}

#define SIP_ROUND(a, b, c, d)             \
    do {                                  \
        (a) += (b); (b) = sip_rotl(b, 13); (b) ^= (a); (a) = sip_rotl(a, 32); \
        (c) += (d); (d) = sip_rotl(d, 16); (d) ^= (c);                        \
        (a) += (d); (d) = sip_rotl(d, 21); (d) ^= (a);                        \
        (c) += (b); (b) = sip_rotl(b, 17); (b) ^= (c); (c) = sip_rotl(c, 32); \
    } while (0)

proven_u64 proven_hash_keyed(proven_mem_view_t data, const proven_byte_t key[16]) {
    proven_u64 k0 = sip_read_le64(key);
    proven_u64 k1 = sip_read_le64(key + 8);

    proven_u64 v0 = 0x736f6d6570736575ull ^ k0;
    proven_u64 v1 = 0x646f72616e646f6dull ^ k1;
    proven_u64 v2 = 0x6c7967656e657261ull ^ k0;
    proven_u64 v3 = 0x7465646279746573ull ^ k1;

    if (data.size > 0 && !data.ptr) { data.size = 0; }   /* consistent with the SHA path */
    const proven_byte_t *in = data.ptr;
    proven_size_t len = data.size;
    proven_size_t left = len & 7u;
    const proven_byte_t *end = in + (len - left);

    /* 2 compression rounds ("2") per 8-byte word. */
    for (; in != end; in += 8) {
        proven_u64 m = sip_read_le64(in);
        v3 ^= m;
        SIP_ROUND(v0, v1, v2, v3);
        SIP_ROUND(v0, v1, v2, v3);
        v0 ^= m;
    }

    /* Final word: the remaining 0..7 bytes, with the total length in the top byte. */
    proven_u64 b = (proven_u64)(len & 0xffu) << 56;
    for (proven_size_t i = 0; i < left; ++i) {
        b |= (proven_u64)in[i] << (8 * i);
    }
    v3 ^= b;
    SIP_ROUND(v0, v1, v2, v3);
    SIP_ROUND(v0, v1, v2, v3);
    v0 ^= b;

    /* 4 finalization rounds ("4"). */
    v2 ^= 0xff;
    SIP_ROUND(v0, v1, v2, v3);
    SIP_ROUND(v0, v1, v2, v3);
    SIP_ROUND(v0, v1, v2, v3);
    SIP_ROUND(v0, v1, v2, v3);

    return v0 ^ v1 ^ v2 ^ v3;
}

// =============================================================================
// CRC-32 (IEEE 802.3, reflected) - corruption checksum
// =============================================================================

/* The reflected CRC-32 table (polynomial 0xEDB88320): table[n] is the CRC of the single byte
 * n. It replaces the eight-iteration inner loop the bitwise form ran per byte with one lookup,
 * which is what a table is for - measured at ~4x on this machine, and the reason it is 1 KiB of
 * .rodata rather than the 8 KiB a slice-by-8 variant would cost. Generated, and the "123456789"
 * check value below is verified against it. */
static const proven_u32 CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

proven_u32 proven_crc32_update(proven_u32 crc, proven_mem_view_t data) {
    /* Reflected form: init/xorout 0xFFFFFFFF, polynomial 0xEDB88320. The running value the
     * caller holds is the already-inverted CRC, so we invert on the way in and out and the
     * middle stays as a plain register - that is what makes chaining chunks work. */
    proven_u32 c = ~crc;
    if (data.size > 0 && !data.ptr) return crc;   /* consistent with the SHA path */
    for (proven_size_t i = 0; i < data.size; ++i) {
        c = (c >> 8) ^ CRC32_TABLE[(c ^ (proven_u32)data.ptr[i]) & 0xFFu];
    }
    return ~c;
}

proven_u32 proven_crc32(proven_mem_view_t data) {
    return proven_crc32_update(0, data);
}

// =============================================================================
// SHA-256 (FIPS 180-4) - cryptographic digest
// =============================================================================

static const proven_u32 sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static proven_u32 sha_rotr(proven_u32 x, int n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_compress(proven_u32 state[8], const proven_byte_t block[64]) {
    proven_u32 w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((proven_u32)block[i * 4] << 24) | ((proven_u32)block[i * 4 + 1] << 16) |
               ((proven_u32)block[i * 4 + 2] << 8) | (proven_u32)block[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        proven_u32 s0 = sha_rotr(w[i - 15], 7) ^ sha_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        proven_u32 s1 = sha_rotr(w[i - 2], 17) ^ sha_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    proven_u32 a = state[0], b = state[1], c = state[2], d = state[3];
    proven_u32 e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        proven_u32 S1 = sha_rotr(e, 6) ^ sha_rotr(e, 11) ^ sha_rotr(e, 25);
        proven_u32 ch = (e & f) ^ (~e & g);
        proven_u32 t1 = h + S1 + ch + sha256_k[i] + w[i];
        proven_u32 S0 = sha_rotr(a, 2) ^ sha_rotr(a, 13) ^ sha_rotr(a, 22);
        proven_u32 maj = (a & b) ^ (a & c) ^ (b & c);
        proven_u32 t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void proven_sha256_init(proven_sha256_t *ctx) {
    if (!ctx) return;
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
    ctx->length = 0;
    ctx->block_len = 0;
}

void proven_sha256_update(proven_sha256_t *ctx, proven_mem_view_t data) {
    if (!ctx || (data.size > 0 && !data.ptr)) return;
    ctx->length += data.size;
    for (proven_size_t i = 0; i < data.size; ++i) {
        ctx->block[ctx->block_len++] = data.ptr[i];
        if (ctx->block_len == 64) {
            sha256_compress(ctx->state, ctx->block);
            ctx->block_len = 0;
        }
    }
}

void proven_sha256_final(proven_sha256_t *ctx, proven_byte_t out[PROVEN_SHA256_SIZE]) {
    if (!ctx || !out) return;

    /* The length in BITS, captured before the padding is appended. */
    proven_u64 bits = ctx->length * 8u;

    /* Append 0x80, then zeros, until the block is 56 bytes - leaving 8 for the length. If
     * there is not room, finish this block first and pad in a fresh one (the case the
     * 56-byte and million-'a' vectors exist to catch). */
    proven_byte_t one = 0x80;
    proven_sha256_update(ctx, (proven_mem_view_t){ .ptr = &one, .size = 1 });
    /* proven_sha256_update bumped length; it does not matter, `bits` was already taken. */
    proven_byte_t zero = 0;
    while (ctx->block_len != 56) {
        proven_sha256_update(ctx, (proven_mem_view_t){ .ptr = &zero, .size = 1 });
    }

    proven_byte_t lenbytes[8];
    for (int i = 0; i < 8; ++i) lenbytes[i] = (proven_byte_t)(bits >> (56 - 8 * i));
    proven_sha256_update(ctx, (proven_mem_view_t){ .ptr = lenbytes, .size = 8 });
    /* block_len is now 0: the last block was compressed by the final update. */

    for (int i = 0; i < 8; ++i) {
        out[i * 4]     = (proven_byte_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (proven_byte_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (proven_byte_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (proven_byte_t)(ctx->state[i]);
    }
}

void proven_sha256(proven_mem_view_t data, proven_byte_t out[PROVEN_SHA256_SIZE]) {
    proven_sha256_t ctx;
    proven_sha256_init(&ctx);
    proven_sha256_update(&ctx, data);
    proven_sha256_final(&ctx, out);
}

void proven_sha256_to_hex(const proven_byte_t digest[PROVEN_SHA256_SIZE], char out[65]) {
    static const char d[] = "0123456789abcdef";
    if (!digest || !out) return;
    for (proven_size_t i = 0; i < PROVEN_SHA256_SIZE; ++i) {
        out[i * 2]     = d[digest[i] >> 4];
        out[i * 2 + 1] = d[digest[i] & 0xf];
    }
    out[64] = 0;
}
