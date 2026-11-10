#include "proven_internal_crypto.h"
#include "proven/hmac.h"

/* P-256 and P-384: ECDH, ECDSA signing and verification.
 *
 * Points are projective (X : Y : Z) with coordinates in Montgomery form, and are added and
 * doubled with the complete formulas of Renes, Costello and Batina (2016) for a curve whose
 * a is -3, as both of these are: algorithm 4 (addition), 5 (addition of a point given in
 * affine coordinates) and 6 (doubling). "Complete" is the property this unit is built on:
 * one formula is correct for every pair of points - equal, opposite, the point at infinity -
 * so there is no special case to branch into, and a scalar multiplication does the same
 * sequence of operations for every scalar.
 *
 * A multiple of the generator - a public key, the first half of a signature - is taken from
 * a table of sixteen fixed points a curve (a signed comb of five teeth), which leaves a fifth
 * of the doublings. A multiple of any other point walks a table made for that point.
 *
 * Signature verification is the exception to all of this, and only it: it has no secret, and
 * takes its two multiples in one variable-time pass with formulas that are not complete (the
 * section "Verification, in variable time" below). */

#define EC_LIMBS PROVEN_CRYPTO_MP_LIMBS(PROVEN_CRYPTO_EC_MAX_BYTES)   /* P-384 */
#define EC_NIBBLES (PROVEN_CRYPTO_MP_LIMB_BITS / 4)                  /* in a limb */

typedef struct {
    proven_size_t limbs;
    proven_size_t bytes;
    proven_crypto_mp_mod_t p;                     /* the field */
    proven_crypto_mp_mod_t n;                     /* the group order */
    proven_crypto_limb_t a[EC_LIMBS];                       /* -3, Montgomery form: the curve check alone uses it */
    proven_crypto_limb_t b[EC_LIMBS];
    proven_crypto_limb_t one[EC_LIMBS];
    proven_crypto_limb_t gx[EC_LIMBS], gy[EC_LIMBS];
    const proven_byte_t *gtab;                              /* EC_GTAB_POINTS entries of 2 * bytes: x then y, plain, big-endian */
    bool p256;                                              /* the field code of P-256 alone may be used */
} proven_ec_curve_t;

#define EC_GTAB_POINTS 16
#define EC_COMB_TEETH 5

typedef struct { proven_crypto_limb_t x[EC_LIMBS], y[EC_LIMBS], z[EC_LIMBS]; } proven_ec_point_t;

static const proven_byte_t EC_P256_P[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
static const proven_byte_t EC_P256_N[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51 };
static const proven_byte_t EC_P256_B[32] = {
    0x5a,0xc6,0x35,0xd8,0xaa,0x3a,0x93,0xe7,0xb3,0xeb,0xbd,0x55,0x76,0x98,0x86,0xbc,
    0x65,0x1d,0x06,0xb0,0xcc,0x53,0xb0,0xf6,0x3b,0xce,0x3c,0x3e,0x27,0xd2,0x60,0x4b };
static const proven_byte_t EC_P256_GX[32] = {
    0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,
    0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96 };
static const proven_byte_t EC_P256_GY[32] = {
    0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,
    0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5 };

static const proven_byte_t EC_P384_P[48] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff };
static const proven_byte_t EC_P384_N[48] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc7,0x63,0x4d,0x81,0xf4,0x37,0x2d,0xdf,
    0x58,0x1a,0x0d,0xb2,0x48,0xb0,0xa7,0x7a,0xec,0xec,0x19,0x6a,0xcc,0xc5,0x29,0x73 };
static const proven_byte_t EC_P384_B[48] = {
    0xb3,0x31,0x2f,0xa7,0xe2,0x3e,0xe7,0xe4,0x98,0x8e,0x05,0x6b,0xe3,0xf8,0x2d,0x19,
    0x18,0x1d,0x9c,0x6e,0xfe,0x81,0x41,0x12,0x03,0x14,0x08,0x8f,0x50,0x13,0x87,0x5a,
    0xc6,0x56,0x39,0x8d,0x8a,0x2e,0xd1,0x9d,0x2a,0x85,0xc8,0xed,0xd3,0xec,0x2a,0xef };
static const proven_byte_t EC_P384_GX[48] = {
    0xaa,0x87,0xca,0x22,0xbe,0x8b,0x05,0x37,0x8e,0xb1,0xc7,0x1e,0xf3,0x20,0xad,0x74,
    0x6e,0x1d,0x3b,0x62,0x8b,0xa7,0x9b,0x98,0x59,0xf7,0x41,0xe0,0x82,0x54,0x2a,0x38,
    0x55,0x02,0xf2,0x5d,0xbf,0x55,0x29,0x6c,0x3a,0x54,0x5e,0x38,0x72,0x76,0x0a,0xb7 };
static const proven_byte_t EC_P384_GY[48] = {
    0x36,0x17,0xde,0x4a,0x96,0x26,0x2c,0x6f,0x5d,0x9e,0x98,0xbf,0x92,0x92,0xdc,0x29,
    0xf8,0xf4,0x1d,0xbd,0x28,0x9a,0x14,0x7c,0xe9,0xda,0x31,0x13,0xb5,0xf0,0xb8,0xc0,
    0x0a,0x60,0xb1,0xce,0x1d,0x7e,0x81,0x9d,0x7a,0x43,0x1d,0x7c,0x90,0xea,0x0e,0x5f };

/* The generator's table, for a signed comb of five teeth d = ceil(bits / 5) apart (52 for
 * P-256, 77 for P-384). Entry i, for i = 0 to 15, is
 *     2^(4d) G + the sum over b = 0..3 of (+1 if bit b of i is set, else -1) 2^(b d) G,
 * plain affine coordinates. The values were computed by another implementation;
 * proven_crypto_ec_test_generator_table recomputes each with the general code below, and the
 * suite runs it. */
static const proven_byte_t EC_P256_GTAB[16][64] = {
    { /* 0 */
        0x84,0x29,0xdf,0xdd,0x7f,0xd0,0x81,0xc5,0xc7,0x0c,0x60,0xad,0x4a,0xcd,0xa0,0xcb,
        0xf8,0x8c,0x60,0xc8,0x3a,0x22,0xaa,0xd4,0xf9,0x52,0x76,0xd2,0xc7,0xe5,0x4b,0xee,
        0xc0,0x56,0xe5,0x44,0x85,0x18,0x97,0xe6,0x8b,0xae,0x07,0x1e,0x20,0xf9,0xff,0x59,
        0x26,0xd8,0x2c,0x6b,0x13,0x13,0x88,0x32,0xb6,0xe0,0x09,0x49,0x53,0x87,0x30,0x20
    },
    { /* 1 */
        0x2e,0x8f,0x84,0xba,0xdf,0x5a,0xd6,0xe9,0x74,0xa9,0x5e,0x72,0x92,0xd8,0x1b,0x88,
        0xbd,0x56,0x92,0x74,0x2a,0x56,0x6f,0xc8,0xaa,0x44,0x31,0x4c,0xea,0x4b,0x56,0x4a,
        0xd4,0x4a,0xc5,0x5d,0x54,0x38,0xfb,0xad,0x45,0xda,0x91,0x65,0xcd,0x48,0x2e,0xca,
        0x41,0x1f,0x1c,0xcd,0xb1,0x58,0x43,0xf8,0xd3,0xf6,0xbb,0xe9,0x93,0x5c,0x5d,0xad
    },
    { /* 2 */
        0x57,0x2d,0x60,0x7b,0x2a,0xc9,0xce,0x9f,0xeb,0x66,0x2c,0xf0,0x7d,0xa8,0x1d,0xca,
        0x3b,0x08,0x6f,0x1f,0x36,0xe6,0x5e,0xb5,0x0e,0x64,0x5a,0xc3,0x16,0x74,0xdc,0xab,
        0x12,0x5e,0xb4,0xaa,0x1b,0xcc,0x94,0x55,0x15,0x28,0xeb,0x2d,0xdd,0x40,0xce,0x47,
        0x5f,0x60,0x20,0xd9,0xe1,0x45,0x1f,0x4e,0xda,0xc5,0xf4,0xc1,0x25,0xdd,0xa5,0x60
    },
    { /* 3 */
        0xa4,0xf9,0xf3,0x67,0x2c,0x06,0xe4,0xea,0xbe,0x4f,0xa3,0x09,0x25,0x0a,0x69,0x32,
        0x7b,0x6d,0x23,0x4e,0xc3,0xda,0x30,0xbb,0x41,0x61,0x83,0x05,0xbc,0xb7,0x05,0x52,
        0xf7,0x6f,0x53,0x48,0x25,0xc4,0x42,0xe4,0x5a,0xf9,0x50,0x1f,0xa5,0xd9,0x8e,0x06,
        0x90,0x09,0x7c,0xb6,0x05,0x2a,0x14,0xae,0xb8,0xeb,0xea,0x26,0xf6,0x8d,0x98,0x1b
    },
    { /* 4 */
        0x3f,0x0c,0x96,0xe6,0xcf,0xef,0x22,0x13,0x4c,0xfb,0x0e,0x28,0x64,0x6d,0xb6,0x07,
        0x89,0xae,0x78,0x8c,0x22,0xbd,0x69,0x11,0xba,0x93,0x14,0xd9,0x33,0x8e,0x58,0xda,
        0xdb,0xfb,0x71,0x91,0x92,0xcc,0xc3,0x5c,0x15,0xf3,0xf0,0x2b,0x21,0x87,0x6f,0xc4,
        0xde,0x2e,0x23,0x7a,0xfd,0x66,0x57,0xfa,0xf9,0x66,0xd2,0xb0,0xf3,0x50,0x10,0x83
    },
    { /* 5 */
        0x79,0x18,0xb0,0x3b,0xde,0x55,0xc5,0xed,0xd9,0xfd,0x0d,0xa1,0x64,0x39,0x66,0xb8,
        0x10,0x65,0xae,0x57,0xcc,0x8e,0xa3,0x58,0x3e,0x95,0x56,0x41,0xb2,0x58,0xfb,0xba,
        0x61,0x4a,0xf4,0x53,0x51,0xea,0x04,0x8b,0xfb,0x2b,0x86,0x3e,0xcd,0xdb,0x93,0x09,
        0x54,0x3b,0x7d,0xd0,0x8e,0x46,0xe9,0x93,0xbc,0x3b,0xae,0xe5,0xb6,0x87,0x0e,0x88
    },
    { /* 6 */
        0xaf,0x21,0xe4,0x9c,0x72,0xb2,0x24,0x79,0x41,0xfb,0xa4,0x9e,0x8e,0x9e,0x8b,0xf3,
        0xc5,0xd1,0x5a,0x99,0x9b,0x4a,0xd9,0xfd,0x0a,0x3e,0x34,0x94,0x10,0x32,0x66,0x11,
        0xb0,0x11,0x0f,0xe5,0xfc,0xae,0x53,0x6c,0xd2,0x00,0xd6,0xff,0xcf,0x1d,0x41,0x05,
        0xd1,0x43,0xd5,0x9d,0x3e,0xa1,0x11,0x6a,0xf9,0x41,0x49,0x62,0x13,0xa4,0xb5,0x2a
    },
    { /* 7 */
        0xd5,0x58,0x8a,0x0d,0xc9,0x48,0x56,0x1b,0x98,0x46,0x5d,0xd3,0xf4,0x7b,0xf8,0xea,
        0x0f,0x09,0x1a,0x2f,0x86,0xfb,0x87,0x97,0xcf,0x04,0x27,0x14,0x99,0x4a,0x5b,0x6e,
        0xda,0xa9,0x4e,0x8f,0xa3,0x5c,0x55,0x1a,0xe9,0xf6,0x49,0xda,0xc7,0xf7,0xa9,0x2f,
        0x47,0xf5,0xcb,0x7d,0x42,0xdd,0xc4,0x96,0xde,0x5b,0x9a,0x41,0x9b,0xc7,0x49,0x03
    },
    { /* 8 */
        0x01,0xe0,0xde,0x74,0xd8,0x6d,0x9b,0xc0,0x59,0x24,0xf7,0xf0,0x90,0xe7,0xf7,0xf9,
        0xa8,0xea,0x75,0x89,0x4d,0x6e,0x17,0x37,0x09,0x68,0xaa,0xa0,0x9c,0x6d,0xe2,0xf0,
        0xdb,0xbd,0x89,0x1f,0x48,0x8b,0x2f,0x0a,0xdb,0xb4,0xca,0x96,0x0a,0xa4,0x4e,0x5d,
        0x51,0x22,0x67,0xad,0x4a,0x0a,0x4a,0xef,0x9b,0x06,0xbf,0x92,0x68,0xaf,0x55,0x2b
    },
    { /* 9 */
        0x2b,0x51,0xf0,0x9a,0x3b,0xd7,0x36,0x79,0xdb,0x3c,0xd8,0xf0,0x79,0xc1,0xa3,0xab,
        0xa0,0x7e,0xde,0xc9,0x98,0x05,0x68,0x27,0xe7,0xda,0x7a,0x30,0x3e,0xf6,0xf4,0xc1,
        0xad,0x9c,0xe7,0xab,0x33,0x2a,0xb9,0x12,0x96,0x6b,0x6b,0xd4,0x09,0x31,0x50,0x57,
        0x61,0xa5,0x24,0xf3,0xdf,0xd9,0xfe,0x28,0x6b,0x4b,0xa1,0x9f,0xa4,0x5f,0x02,0xe8
    },
    { /* 10 */
        0xeb,0x34,0xf0,0x80,0x24,0x37,0x3a,0x17,0xd3,0x31,0xbc,0xdc,0xc3,0xf5,0xec,0xec,
        0xae,0x16,0x00,0xab,0xc0,0x01,0x57,0xb9,0x0a,0xbb,0x92,0x6b,0x85,0x45,0x43,0x8a,
        0x75,0x8f,0xe2,0x59,0xa8,0xf9,0x30,0x55,0x5f,0xe2,0x4b,0xa3,0xaa,0xdb,0x79,0x2e,
        0xf0,0x2c,0xa1,0x0a,0xcf,0x0d,0x91,0xcd,0x57,0x10,0x00,0x75,0xb1,0xef,0x8e,0x14
    },
    { /* 11 */
        0xda,0xb8,0xdd,0xdc,0x24,0xda,0x64,0x38,0x1a,0xeb,0xf4,0x3c,0xdd,0x9e,0xbe,0x66,
        0x0c,0x0b,0xf6,0x13,0x8b,0x38,0x43,0xd5,0x3b,0x9e,0x5a,0x25,0x32,0x03,0x04,0xd1,
        0x9e,0xb0,0xef,0xbf,0x9e,0x4e,0x37,0x0c,0x76,0x50,0xec,0x55,0x8d,0x31,0x5e,0xf7,
        0x64,0x77,0x97,0xc6,0x48,0xca,0x98,0x37,0xf6,0x54,0x1c,0x56,0x08,0xba,0x5b,0x92
    },
    { /* 12 */
        0x52,0x78,0x84,0x40,0x96,0xc4,0xff,0x9c,0x89,0x61,0x6c,0xb1,0xe7,0x9e,0x07,0xdd,
        0xdc,0x8f,0x13,0xbf,0xca,0xed,0xdb,0x83,0x8c,0x3d,0x52,0x02,0x79,0x8f,0x31,0x6d,
        0x4d,0x3d,0x59,0x23,0xa6,0x1a,0xe1,0xb4,0xde,0xd0,0xd1,0x5b,0xbf,0x2d,0xd8,0x34,
        0x80,0xb8,0x66,0xfe,0x6c,0x50,0xa1,0xef,0xa2,0x09,0x99,0xf6,0xa9,0x34,0xb6,0x69
    },
    { /* 13 */
        0x79,0x2f,0x29,0xf8,0x67,0x69,0x84,0xa9,0x4f,0x52,0x39,0xd9,0x79,0x15,0x51,0xab,
        0xc2,0x95,0x20,0xb8,0xbf,0x0a,0xb9,0x11,0xf3,0x17,0xd3,0x2c,0x9b,0xf1,0x74,0xbf,
        0xf0,0xd8,0xce,0x8b,0x3b,0x28,0xe9,0x4a,0x35,0x6f,0xdd,0x6d,0xc9,0xd4,0xb1,0xc1,
        0x9a,0xb2,0xfa,0xf2,0x39,0xb9,0x6d,0x8b,0x08,0xf2,0x67,0xf2,0xa6,0xfb,0x03,0x6b
    },
    { /* 14 */
        0xad,0x7f,0x1b,0x10,0xb6,0x32,0x36,0x54,0xab,0x7d,0x90,0x07,0x69,0xdd,0xab,0xe5,
        0x1c,0x28,0xa6,0x36,0xd0,0x74,0x61,0x91,0xf1,0xb2,0xfb,0x60,0x2c,0x26,0x03,0xd7,
        0x12,0x7f,0x9b,0x5d,0x07,0xbd,0xe2,0x55,0x3f,0xac,0xbe,0x89,0xfd,0xe4,0x78,0x3f,
        0x4a,0x77,0x65,0xa1,0xbe,0x18,0x1b,0xea,0x09,0xb9,0xd1,0x96,0x16,0xbc,0xeb,0x7d
    },
    { /* 15 */
        0x82,0x5f,0x01,0x94,0x8e,0x83,0x1d,0x5b,0x76,0xc4,0xc1,0x80,0x42,0x86,0xfb,0x42,
        0x1a,0x25,0x30,0xb0,0x5a,0x00,0x16,0x9c,0x2e,0x75,0xa2,0x66,0x5b,0x69,0x65,0x27,
        0x43,0x58,0x72,0xfe,0xbc,0x72,0x3a,0x17,0x61,0x79,0x4c,0x4f,0x24,0x11,0x11,0x50,
        0x10,0x6f,0x9b,0xc4,0xce,0x5b,0x10,0x6a,0xdb,0xf0,0xa1,0x1f,0xef,0x70,0x37,0x39
    }
};
static const proven_byte_t EC_P384_GTAB[16][96] = {
    { /* 0 */
        0x84,0x4d,0x1e,0x18,0x2d,0xbc,0x97,0xb6,0x99,0x8a,0x97,0xe0,0x6e,0x77,0x22,0x71,
        0xde,0x7e,0xc4,0xf6,0x95,0xf1,0xc2,0xa9,0x38,0x2c,0x10,0xc8,0x79,0x46,0x0e,0x15,
        0xce,0xa9,0x4a,0xdd,0xb6,0x69,0x15,0x19,0x4c,0x3d,0x58,0x11,0xa0,0x45,0x1c,0x3c,
        0x19,0x40,0x26,0x95,0x92,0xd9,0x2f,0x18,0x32,0x46,0x9e,0x18,0x47,0x92,0x9d,0x92,
        0x1b,0x90,0x01,0x28,0x8f,0xe6,0xb9,0xe0,0x55,0xda,0xf5,0xa9,0x3e,0x4b,0x16,0x56,
        0x02,0x6a,0x47,0x8f,0x2c,0x1d,0xbf,0xb1,0x6d,0x42,0x35,0x33,0xab,0xc2,0xf1,0x19
    },
    { /* 1 */
        0x2b,0x73,0x63,0x19,0xd1,0x90,0x3d,0xef,0xf7,0xc3,0x43,0x87,0xa0,0xde,0xf4,0xfb,
        0x08,0x42,0xb9,0x90,0xfa,0x67,0xf4,0x0f,0xbf,0x14,0xc9,0x0b,0x6b,0xbd,0x1d,0xa4,
        0x7f,0x59,0xd9,0x01,0x03,0x58,0x26,0x6a,0x13,0x9d,0xc5,0xe5,0xc2,0x44,0x27,0x24,
        0xc5,0xef,0x58,0xe6,0xd5,0xea,0x67,0xb2,0x0a,0x5f,0xd8,0x4b,0x7e,0xa8,0x52,0xf4,
        0xa4,0x93,0x9f,0x16,0xc0,0xfa,0xdc,0xe3,0x8b,0x05,0x01,0x32,0x7e,0xc8,0x6a,0x6c,
        0xf7,0xb5,0x4e,0x04,0xdd,0x89,0x23,0x63,0xf0,0xb2,0xa2,0xa3,0x01,0x17,0x6c,0x16
    },
    { /* 2 */
        0xde,0xca,0x9c,0xe7,0x80,0x43,0x7f,0x9b,0x05,0x15,0xb3,0x53,0xf8,0x06,0x7e,0x9a,
        0x6b,0x59,0x1a,0xfb,0x26,0x0a,0x63,0xb5,0x82,0x58,0xc9,0xf0,0xba,0x9a,0x45,0x2b,
        0x17,0xc4,0x19,0x5b,0xe8,0x2c,0xef,0x62,0x1e,0x05,0x92,0x08,0xc8,0xff,0x92,0xb3,
        0xa8,0xa6,0x0e,0x57,0xd4,0x23,0xf5,0xb9,0x7b,0xfc,0xee,0x7c,0xea,0x51,0x95,0x31,
        0x22,0xb3,0x2a,0x03,0x6d,0x99,0x67,0xfa,0x6b,0xe5,0x72,0x91,0xa6,0x3f,0xb5,0x74,
        0xf0,0xab,0xc3,0x36,0xa8,0x03,0x29,0xaf,0x5a,0x11,0xcb,0x7b,0x85,0x49,0xfb,0xac
    },
    { /* 3 */
        0xc1,0x87,0xa7,0x63,0xcb,0xec,0x7e,0xf0,0xfb,0x0c,0x80,0x28,0x49,0x40,0x6e,0x48,
        0xf7,0xb0,0x0f,0xa9,0xcd,0xba,0x28,0x58,0xe5,0xea,0x31,0xcd,0x9f,0xb3,0xda,0xfc,
        0xed,0xa5,0xdc,0x70,0x1e,0x33,0xc9,0x78,0xc4,0x9c,0x93,0xf7,0xc0,0xc5,0x64,0x2f,
        0xb2,0x0c,0x5c,0x85,0xc2,0x6e,0x7c,0x2e,0xd8,0xb1,0x02,0x1c,0xa7,0x45,0x4f,0xe7,
        0xc8,0x55,0x88,0x7d,0x4a,0x3d,0x4e,0x99,0x49,0x6e,0x88,0x82,0xd0,0x92,0xc5,0x52,
        0xca,0xfd,0x8b,0xd2,0x3f,0xe8,0xc2,0xd4,0x75,0x0b,0x15,0xfd,0xfc,0xf6,0xe7,0x6a
    },
    { /* 4 */
        0x90,0x83,0x2e,0xb2,0x06,0xd5,0x5c,0x11,0xdf,0x5e,0x80,0x41,0x34,0x40,0x6b,0x6d,
        0x65,0x1b,0x26,0xb0,0x79,0x30,0x21,0x0a,0x34,0xea,0xec,0x4d,0xe7,0xcd,0x55,0xd7,
        0xe0,0xce,0xe4,0xbc,0xde,0x8c,0x94,0xd8,0xf8,0x75,0xaa,0x79,0x1b,0x7a,0x42,0xc2,
        0x7b,0xb8,0x0d,0xa2,0xb4,0x28,0xfa,0x83,0xcd,0x44,0xdf,0x7e,0x1e,0x89,0xa9,0x03,
        0xc1,0xcb,0xd3,0x9e,0x3a,0xf4,0x80,0x12,0xa4,0xea,0x0c,0x98,0x97,0x5b,0xc2,0xfb,
        0xfc,0x8d,0x8c,0x20,0xad,0x67,0xf3,0xf3,0x99,0x44,0xc3,0xec,0xc7,0x0b,0x0e,0x66
    },
    { /* 5 */
        0x18,0x53,0xa7,0x28,0x5d,0xdd,0x8b,0x13,0xfa,0xb5,0x81,0xd6,0xd4,0x7f,0xee,0xd0,
        0xc2,0x90,0x6f,0x35,0x79,0xff,0xe7,0x0c,0x25,0xd4,0x58,0x6f,0x59,0xae,0x96,0x4c,
        0x0b,0x83,0x93,0x3e,0x79,0xc4,0x62,0x89,0x02,0x2d,0xf5,0x7e,0xf9,0x73,0x8b,0xe3,
        0x3b,0x23,0x1d,0xac,0x8b,0x1f,0x97,0x26,0x48,0xe5,0xaf,0xdb,0xf9,0x2c,0xd5,0xfb,
        0xee,0xbe,0xa3,0x13,0xc8,0x6e,0x4c,0xb7,0x19,0xf7,0x5e,0x6f,0x07,0x64,0x45,0xc4,
        0x0e,0x06,0x68,0xb1,0xd8,0x68,0xd2,0x5b,0x6c,0xc9,0xc2,0x0d,0xdc,0x94,0xc2,0xb5
    },
    { /* 6 */
        0xcb,0x5a,0x08,0xde,0x0b,0x19,0xce,0x3d,0x8c,0x2b,0x9b,0xe0,0x1d,0xef,0x22,0xa8,
        0x9f,0x52,0xf5,0xc1,0xf0,0xe7,0x50,0xa3,0xf1,0xbf,0x96,0x65,0x6e,0xb2,0x76,0xff,
        0xf3,0x17,0xf5,0x8a,0x21,0x15,0x27,0xe8,0xb0,0x6e,0x2b,0x57,0xd8,0x52,0xfe,0xed,
        0x60,0xe5,0xcd,0x53,0xbd,0xd6,0x4c,0x9d,0x19,0xa6,0xae,0x15,0xe7,0x3a,0x9a,0x81,
        0xb7,0xdf,0x75,0x96,0x09,0xdd,0x1a,0x3e,0xdb,0xf2,0x7b,0x9e,0xb9,0x30,0x3a,0x92,
        0x8f,0x42,0xe0,0x5e,0xff,0x40,0x66,0x47,0x86,0x80,0xec,0xe9,0xb5,0x82,0xd8,0xe6
    },
    { /* 7 */
        0xad,0xe5,0x2a,0x01,0x69,0x8e,0x32,0x60,0x03,0x84,0x3e,0x08,0x75,0x60,0x6f,0x48,
        0xc3,0x44,0x53,0x55,0x88,0xa9,0x51,0x5e,0x41,0x4b,0xd2,0xcc,0x9d,0x92,0x2d,0xcc,
        0x3c,0x67,0xaf,0xd2,0x03,0xf2,0xc1,0xd0,0x94,0x55,0x80,0x01,0x67,0x1c,0x56,0x41,
        0x63,0xe1,0x7d,0x2a,0x64,0xbf,0x06,0xb2,0xb5,0xad,0x2c,0x1a,0xae,0xcf,0xdf,0x9a,
        0xf4,0x14,0x37,0x60,0x52,0x6e,0x19,0xcf,0x86,0xa3,0x65,0xd3,0xe9,0x35,0x62,0x53,
        0x9c,0xde,0xb7,0xdf,0x4f,0x94,0xf8,0x09,0x60,0x9c,0xdf,0x32,0x97,0x94,0xd1,0x21
    },
    { /* 8 */
        0x30,0x40,0xd5,0x54,0xb4,0xfc,0xec,0x5e,0xa4,0x05,0xec,0xe9,0xb0,0x8d,0x22,0x3e,
        0xce,0xc4,0x30,0xa8,0x1a,0x39,0x9e,0xfd,0xfb,0xfa,0xe0,0x11,0xdf,0xd6,0xa0,0xf8,
        0x60,0x42,0x3e,0xf9,0x18,0x51,0x20,0x9c,0x72,0x2d,0x6e,0xd6,0x71,0xc1,0x74,0xa0,
        0xfb,0x26,0x8c,0x65,0xec,0xbf,0x42,0x22,0x76,0xf7,0x22,0x1f,0x7f,0xaa,0x35,0xa8,
        0xba,0xf8,0xa1,0x5f,0x35,0x49,0x0e,0x0d,0xe6,0x48,0xdf,0x63,0xab,0x7b,0x7e,0x58,
        0xf0,0x03,0x18,0xa4,0xb9,0x43,0x99,0xfb,0x65,0x9d,0xa7,0xf7,0xa7,0x3d,0x13,0x17
    },
    { /* 9 */
        0xcf,0xb0,0x87,0x31,0xe7,0x27,0xef,0xb7,0x6e,0x3c,0x1f,0x6f,0xf5,0x9e,0x22,0x11,
        0xc3,0x76,0x3e,0x48,0x87,0x61,0xec,0x44,0x9c,0x16,0xbc,0xd1,0xad,0x4f,0x7a,0x45,
        0xf9,0xe5,0x37,0x3c,0xec,0xe4,0x64,0x46,0xfa,0x8b,0xf9,0x5b,0x42,0xbc,0x52,0x44,
        0x6a,0xa5,0xaa,0xef,0x5d,0xe2,0xc4,0x94,0x5e,0xbe,0xa8,0x51,0xe6,0x1d,0xc5,0x7b,
        0xd4,0x1d,0x26,0x0b,0x89,0x63,0x30,0x88,0xc1,0x4e,0xd3,0x26,0xe4,0x48,0x8d,0xc7,
        0x2c,0x8d,0x34,0xec,0xb2,0x6a,0x46,0xc3,0xb6,0xa5,0x0f,0x58,0x5d,0x48,0x60,0x45
    },
    { /* 10 */
        0x72,0x1a,0xf7,0x6a,0x41,0x05,0x7b,0x31,0x1a,0x74,0x57,0xcb,0xb7,0x87,0x89,0x84,
        0xf1,0x4f,0xaf,0xf4,0x99,0xe5,0xad,0xc2,0xff,0xc2,0xf0,0x73,0x09,0x8c,0x67,0x69,
        0x8e,0xc5,0xe6,0x99,0xad,0xe7,0x8c,0x80,0x0e,0x35,0x03,0x6d,0x94,0xf2,0x97,0xab,
        0x59,0x19,0x70,0x9b,0x34,0x06,0xbb,0xe4,0x81,0x6e,0x54,0xaa,0x81,0xd8,0xe1,0x07,
        0xb2,0xfa,0xd8,0x86,0x6b,0xa5,0x0b,0xef,0xec,0xfe,0x25,0x35,0x6a,0xdb,0x83,0xe0,
        0x59,0xda,0x09,0x7a,0x4b,0x99,0x0c,0xc2,0x66,0x8f,0xe5,0xf7,0x3f,0xfa,0xbb,0xcb
    },
    { /* 11 */
        0x5a,0x86,0x15,0x3a,0x85,0x7e,0x01,0xe4,0x25,0x6f,0x29,0x97,0xb5,0xe6,0xad,0x5c,
        0xd9,0x9c,0x94,0x65,0x74,0x8d,0x72,0x79,0x0a,0x7c,0xf1,0x53,0x41,0x99,0xb5,0x83,
        0x4e,0x9a,0x7d,0xf0,0x28,0x24,0xfa,0x5e,0x5c,0x10,0x2c,0x92,0x05,0xbf,0x56,0x03,
        0xbc,0x02,0x46,0x71,0x81,0xae,0xb0,0x14,0x2a,0x01,0x41,0xb0,0xa3,0x3a,0xbb,0x66,
        0x2f,0x0a,0xf5,0x53,0x84,0xe0,0x25,0x6c,0x73,0x4e,0xc7,0xf6,0x3e,0x7b,0x9c,0x04,
        0xbc,0xa3,0xae,0x17,0xa7,0xf7,0xdd,0x1f,0x4e,0x86,0x50,0x22,0xd9,0x9b,0x8e,0xc6
    },
    { /* 12 */
        0x93,0xdf,0xd4,0xd7,0xd8,0x6c,0x6e,0xa6,0xcf,0xc0,0x67,0xe4,0x9b,0x4a,0xfe,0x2d,
        0x77,0x23,0xae,0x4a,0x6b,0x07,0xa5,0x2c,0x6b,0x9a,0xac,0x6b,0xf2,0x6b,0xfc,0x86,
        0x01,0x11,0xf8,0xf6,0xa0,0x36,0x26,0x69,0x59,0x5c,0x08,0x6d,0xea,0x49,0x71,0x74,
        0x30,0xa0,0x7b,0xd5,0xd4,0x96,0xd9,0x48,0x93,0x50,0x7b,0x6a,0x61,0x31,0x70,0x9d,
        0xf3,0xdb,0x3f,0xc1,0x48,0x58,0xca,0x3e,0x09,0x10,0x5d,0x4a,0x33,0x2a,0x35,0x0d,
        0x31,0xee,0x5c,0xbd,0x8e,0x41,0x38,0xb3,0x65,0xe1,0xe3,0xae,0x07,0x71,0xbd,0x37
    },
    { /* 13 */
        0xb9,0x96,0xdb,0x7a,0xdc,0xe2,0x9d,0x2c,0x3a,0x45,0xa0,0x26,0x8f,0x3b,0xc7,0x7f,
        0x6c,0xd7,0xea,0xae,0x70,0xab,0x9d,0x71,0xff,0xf7,0x30,0x12,0xc4,0xf7,0xed,0x22,
        0x2e,0x37,0x8f,0x2b,0xab,0x3b,0x49,0x4b,0xf3,0x74,0x10,0x8e,0x77,0xc0,0x67,0x9e,
        0x59,0x78,0x17,0x0d,0x5b,0x48,0x9a,0x2c,0xb3,0xd7,0xf8,0xb0,0x24,0xd4,0xc6,0xbf,
        0xaf,0xa6,0xe7,0x1a,0x40,0xe3,0x07,0x7c,0xef,0x8d,0x34,0x1c,0x97,0xf7,0xac,0xae,
        0x74,0x34,0x2e,0x29,0xdb,0x3c,0x77,0x09,0x8d,0x2f,0x92,0xb1,0x9e,0x7f,0x7c,0xc3
    },
    { /* 14 */
        0x68,0xe7,0x34,0xb4,0x81,0x83,0xb9,0x0e,0x3b,0x20,0x7d,0x50,0xf0,0x78,0x6f,0xd2,
        0x08,0x85,0xc3,0x36,0xba,0x6e,0x76,0x0b,0x88,0x1c,0xb3,0xda,0x09,0xc1,0x08,0x4a,
        0x85,0x84,0x29,0x93,0x25,0x89,0x28,0xe4,0x29,0xf6,0x3e,0x66,0xf2,0x5a,0xd6,0x2d,
        0xb1,0xb9,0xcf,0xd2,0x6c,0x63,0x88,0x30,0x04,0xfa,0x46,0xdd,0x62,0xe2,0x3d,0x33,
        0x8c,0xe3,0x05,0x69,0x2c,0x2f,0x42,0xbd,0x49,0xd5,0x4a,0x7d,0x27,0x85,0xd0,0x0d,
        0xae,0x98,0x5d,0x44,0x5e,0x5b,0xe9,0x2b,0xaa,0xec,0x3d,0xe1,0xc9,0x88,0x2a,0xd3
    },
    { /* 15 */
        0x25,0xe2,0x09,0xc5,0x9d,0xa9,0x12,0x1c,0x15,0x79,0x81,0x46,0x91,0xdb,0xe6,0x68,
        0xe8,0xc6,0x63,0xc5,0xcb,0x12,0x99,0xc9,0xfd,0xa0,0x47,0xeb,0x72,0x49,0x57,0x66,
        0x31,0xb9,0x2b,0x91,0x7f,0xa0,0x18,0x80,0x96,0xed,0xf5,0x0f,0x67,0x9a,0x2a,0xba,
        0x55,0x32,0x00,0xb9,0xda,0x09,0xfb,0x6a,0xda,0x53,0xb0,0x69,0xfe,0x5d,0x66,0x97,
        0x6c,0x27,0x90,0x54,0x6e,0x6c,0xe7,0x44,0x96,0x05,0x2f,0x28,0xe9,0x10,0x31,0x89,
        0x63,0x66,0xe8,0xf3,0xd8,0x2a,0xdb,0x97,0x9a,0xd0,0x33,0xa2,0xf6,0x9b,0x64,0xda
    }
};

/* ---- The field of P-256 alone (RFC-0028 S-6, RFC-0014 C-7) ----
 *
 * p = 2^256 - 2^224 + 2^192 + 2^96 - 1 in four 64-bit limbs, Montgomery form with R = 2^256 -
 * the representation of the general code, so every value, table and result is the same and
 * either may be used on any value. What the prime's shape gives: -p^-1 mod 2^64 is 1, so the
 * multiple of p a reduction step adds is the lowest limb itself; that limb's product with p's
 * lowest limb (2^64 - 1) only carries the limb on, the next (2^32 - 1) is a shift, the third
 * is zero - one multiplication a step where the general code has four. Fixed sizes, no loops
 * the compiler must count: a product is sixteen multiplications and the reduction four, a
 * square ten and four. Branch-free throughout, as the general code. 64-bit limbs only.
 *
 * The carries are 128-bit sums, written out: branch-free at every optimisation level with every
 * compiler, and GCC 16 and Clang make good add-with-carry chains of them. (Tried and dropped,
 * RFC-0028 S-6: GCC's __builtin_addcll, which becomes conditional jumps on the values at -O0
 * and -Og - ct-poison found them; carries as comparisons, slower; helper functions for the
 * carries, and forcing the field functions inline, both slower with GCC 16.) The product, the
 * square and the reduction work on named words rather than arrays (v0.101.0): GCC 14 kept the
 * arrays in memory through their loops, 0.78-0.83 of the time with it, 0.87-0.93 with GCC 16,
 * the same with Clang. */

#if PROVEN_CRYPTO_MP_LIMB_BITS == 64 && defined(__SIZEOF_INT128__)
#define EC_P256_FIELD 1
__extension__ typedef unsigned __int128 ec_u128;
#define P256_P3 0xffffffff00000001ull
static const proven_u64 P256_P[4] = { 0xffffffffffffffffull, 0x00000000ffffffffull, 0, P256_P3 };

/* Every carry is the high half of a 128-bit sum - branch-free at every optimisation level. */
#define P256_MAC(t, x, y, c) do { const ec_u128 v_ = (ec_u128)(x) * (y) + (t) + (c); (t) = (proven_u64)v_; (c) = (proven_u64)(v_ >> 64); } while (0)
/* One step of the reduction: m = t0, the low word, cleared by adding m p, and what it carries
 * into the four words above it (m (2^64 - 1) only carries m on, m (2^32 - 1) plus that is m 2^32,
 * the third limb is zero, the fourth one multiplication). */
#define P256_RED(m, t1, t2, t3, t4, top) do {                                         \
        ec_u128 v_ = (ec_u128)(t1) + ((ec_u128)(m) << 32);                            \
        (t1) = (proven_u64)v_;                                                        \
        v_ = (ec_u128)(t2) + (proven_u64)(v_ >> 64);                                  \
        (t2) = (proven_u64)v_;                                                        \
        v_ = (ec_u128)(t3) + (ec_u128)(m) * P256_P3 + (proven_u64)(v_ >> 64);         \
        (t3) = (proven_u64)v_;                                                        \
        v_ = (ec_u128)(t4) + (proven_u64)(v_ >> 64) + (top);                          \
        (t4) = (proven_u64)v_;                                                        \
        (top) = (proven_u64)(v_ >> 64);                                               \
    } while (0)

/* t0..t7, a value below p^2: t / 2^256 mod p into o, fully reduced. */
static inline void p256_reduce8(proven_u64 o[4], proven_u64 t0, proven_u64 t1, proven_u64 t2, proven_u64 t3,
                                proven_u64 t4, proven_u64 t5, proven_u64 t6, proven_u64 t7) {
    proven_u64 top = 0;
    P256_RED(t0, t1, t2, t3, t4, top);
    P256_RED(t1, t2, t3, t4, t5, top);
    P256_RED(t2, t3, t4, t5, t6, top);
    P256_RED(t3, t4, t5, t6, t7, top);
    /* t4..t7 and top: below 2p. One subtraction, chosen by mask. */
    ec_u128 v = (ec_u128)t4 - P256_P[0];
    const proven_u64 d0 = (proven_u64)v;
    v = (ec_u128)t5 - P256_P[1] - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 d1 = (proven_u64)v;
    v = (ec_u128)t6 - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 d2 = (proven_u64)v;
    v = (ec_u128)t7 - P256_P3 - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 d3 = (proven_u64)v, borrow = (proven_u64)(v >> 64) & 1u;
    const proven_u64 keep = (proven_u64)0 - ((top | (borrow ^ 1u)) & 1u);       /* all ones: take d */
    o[0] = (d0 & keep) | (t4 & ~keep);
    o[1] = (d1 & keep) | (t5 & ~keep);
    o[2] = (d2 & keep) | (t6 & ~keep);
    o[3] = (d3 & keep) | (t7 & ~keep);
}

static void p256_mul(proven_u64 o[4], const proven_u64 a[4], const proven_u64 b[4]) {
    const proven_u64 a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
    proven_u64 t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4, t5, t6, t7, c, bi;
    bi = b[0]; c = 0;
    P256_MAC(t0, a0, bi, c); P256_MAC(t1, a1, bi, c); P256_MAC(t2, a2, bi, c); P256_MAC(t3, a3, bi, c); t4 = c;
    bi = b[1]; c = 0;
    P256_MAC(t1, a0, bi, c); P256_MAC(t2, a1, bi, c); P256_MAC(t3, a2, bi, c); P256_MAC(t4, a3, bi, c); t5 = c;
    bi = b[2]; c = 0;
    P256_MAC(t2, a0, bi, c); P256_MAC(t3, a1, bi, c); P256_MAC(t4, a2, bi, c); P256_MAC(t5, a3, bi, c); t6 = c;
    bi = b[3]; c = 0;
    P256_MAC(t3, a0, bi, c); P256_MAC(t4, a1, bi, c); P256_MAC(t5, a2, bi, c); P256_MAC(t6, a3, bi, c); t7 = c;
    p256_reduce8(o, t0, t1, t2, t3, t4, t5, t6, t7);
}

static void p256_sqr(proven_u64 o[4], const proven_u64 a[4]) {
    const proven_u64 a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
    /* The six products of different limbs once, doubled, then the four squares. */
    proven_u64 t1 = 0, t2 = 0, t3 = 0, t4, t5, t6, t7, c;
    c = 0; P256_MAC(t1, a0, a1, c); P256_MAC(t2, a0, a2, c); P256_MAC(t3, a0, a3, c); t4 = c;
    c = 0; P256_MAC(t3, a1, a2, c); P256_MAC(t4, a1, a3, c); t5 = c;
    c = 0; P256_MAC(t5, a2, a3, c); t6 = c;
    t7 = t6 >> 63;
    t6 = (t6 << 1) | (t5 >> 63); t5 = (t5 << 1) | (t4 >> 63); t4 = (t4 << 1) | (t3 >> 63);
    t3 = (t3 << 1) | (t2 >> 63); t2 = (t2 << 1) | (t1 >> 63); t1 <<= 1;
    proven_u64 t0;
    ec_u128 sq, v;
    sq = (ec_u128)a0 * a0; t0 = (proven_u64)sq;
    v = (ec_u128)t1 + (proven_u64)(sq >> 64); t1 = (proven_u64)v;
    sq = (ec_u128)a1 * a1;
    v = (ec_u128)t2 + (proven_u64)sq + (proven_u64)(v >> 64); t2 = (proven_u64)v;
    v = (ec_u128)t3 + (proven_u64)(sq >> 64) + (proven_u64)(v >> 64); t3 = (proven_u64)v;
    sq = (ec_u128)a2 * a2;
    v = (ec_u128)t4 + (proven_u64)sq + (proven_u64)(v >> 64); t4 = (proven_u64)v;
    v = (ec_u128)t5 + (proven_u64)(sq >> 64) + (proven_u64)(v >> 64); t5 = (proven_u64)v;
    sq = (ec_u128)a3 * a3;
    v = (ec_u128)t6 + (proven_u64)sq + (proven_u64)(v >> 64); t6 = (proven_u64)v;
    v = (ec_u128)t7 + (proven_u64)(sq >> 64) + (proven_u64)(v >> 64); t7 = (proven_u64)v;
    p256_reduce8(o, t0, t1, t2, t3, t4, t5, t6, t7);
}

/* The sum and the difference on named words, each carry the high half of a 128-bit sum, as in
 * p256_reduce8: GCC made 114-119 instructions of the loops over arrays these replace, with a
 * vector select, and kept the function out of line - a quarter to a third of an ECDH's
 * instructions (2026-11-10, hardware counters). (`__builtin_addcll` and `__builtin_subcll` were
 * shorter still, but GCC 14 compiles them to conditional jumps at -O0 and -Og.) */
static void p256_add(proven_u64 o[4], const proven_u64 a[4], const proven_u64 b[4]) {
    ec_u128 v = (ec_u128)a[0] + b[0];
    const proven_u64 r0 = (proven_u64)v;
    v = (ec_u128)a[1] + b[1] + (proven_u64)(v >> 64);
    const proven_u64 r1 = (proven_u64)v;
    v = (ec_u128)a[2] + b[2] + (proven_u64)(v >> 64);
    const proven_u64 r2 = (proven_u64)v;
    v = (ec_u128)a[3] + b[3] + (proven_u64)(v >> 64);
    const proven_u64 r3 = (proven_u64)v, carry = (proven_u64)(v >> 64);
    v = (ec_u128)r0 - P256_P[0];
    const proven_u64 d0 = (proven_u64)v;
    v = (ec_u128)r1 - P256_P[1] - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 d1 = (proven_u64)v;
    v = (ec_u128)r2 - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 d2 = (proven_u64)v;
    v = (ec_u128)r3 - P256_P3 - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 d3 = (proven_u64)v, borrow = (proven_u64)(v >> 64) & 1u;
    const proven_u64 keep = (proven_u64)0 - ((carry | (borrow ^ 1u)) & 1u);    /* all ones: take d */
    o[0] = (d0 & keep) | (r0 & ~keep);
    o[1] = (d1 & keep) | (r1 & ~keep);
    o[2] = (d2 & keep) | (r2 & ~keep);
    o[3] = (d3 & keep) | (r3 & ~keep);
}

static void p256_sub(proven_u64 o[4], const proven_u64 a[4], const proven_u64 b[4]) {
    ec_u128 v = (ec_u128)a[0] - b[0];
    const proven_u64 r0 = (proven_u64)v;
    v = (ec_u128)a[1] - b[1] - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 r1 = (proven_u64)v;
    v = (ec_u128)a[2] - b[2] - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 r2 = (proven_u64)v;
    v = (ec_u128)a[3] - b[3] - ((proven_u64)(v >> 64) & 1u);
    const proven_u64 r3 = (proven_u64)v, mask = (proven_u64)0 - ((proven_u64)(v >> 64) & 1u);  /* add p back when it borrowed */
    v = (ec_u128)r0 + (P256_P[0] & mask);
    o[0] = (proven_u64)v;
    v = (ec_u128)r1 + (P256_P[1] & mask) + (proven_u64)(v >> 64);
    o[1] = (proven_u64)v;
    v = (ec_u128)r2 + (proven_u64)(v >> 64);
    o[2] = (proven_u64)v;
    o[3] = r3 + (P256_P3 & mask) + (proven_u64)(v >> 64);
}

static void p256_sqr_n(proven_u64 o[4], const proven_u64 a[4], int n) {
    p256_sqr(o, a);
    while (--n > 0) p256_sqr(o, o);
}

/* x^(p - 2) = 1 / x (0 for 0): 255 squarings and 12 multiplications over the fixed chain of
 * p - 2's bits (its 32-bit words from the top: FFFFFFFF 00000001 0 0 0 FFFFFFFF FFFFFFFF
 * FFFFFFFD). The exponent is public; the chain is the same for every x. */
static void p256_inv(proven_u64 o[4], const proven_u64 x[4]) {
    proven_u64 x2[4], x3[4], x6[4], x12[4], x15[4], x30[4], x32[4], t[4];
    p256_sqr(t, x); p256_mul(x2, t, x);                    /* 2^2 - 1 */
    p256_sqr(t, x2); p256_mul(x3, t, x);                   /* 2^3 - 1 */
    p256_sqr_n(t, x3, 3); p256_mul(x6, t, x3);             /* 2^6 - 1 */
    p256_sqr_n(t, x6, 6); p256_mul(x12, t, x6);            /* 2^12 - 1 */
    p256_sqr_n(t, x12, 3); p256_mul(x15, t, x3);           /* 2^15 - 1 */
    p256_sqr_n(t, x15, 15); p256_mul(x30, t, x15);         /* 2^30 - 1 */
    p256_sqr_n(t, x30, 2); p256_mul(x32, t, x2);           /* 2^32 - 1: FFFFFFFF */
    p256_sqr_n(t, x32, 32); p256_mul(t, t, x);             /* 00000001 */
    p256_sqr_n(t, t, 96 + 32); p256_mul(t, t, x32);        /* 0 0 0, FFFFFFFF */
    p256_sqr_n(t, t, 32); p256_mul(t, t, x32);             /* FFFFFFFF */
    p256_sqr_n(t, t, 30); p256_mul(t, t, x30);             /* FFFFFFFD: 2^30 - 1, then */
    p256_sqr_n(t, t, 2); p256_mul(o, t, x);                /* times 4, plus 1 */
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x2, .size = sizeof x2 });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x3, .size = sizeof x3 });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x6, .size = sizeof x6 });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x12, .size = sizeof x12 });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x15, .size = sizeof x15 });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x30, .size = sizeof x30 });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x32, .size = sizeof x32 });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)t, .size = sizeof t });
}
#else
#define EC_P256_FIELD 0
#endif

proven_size_t proven_crypto_ec_size(proven_crypto_ec_curve_t curve) {
    return curve == PROVEN_CRYPTO_EC_P256 ? 32 : curve == PROVEN_CRYPTO_EC_P384 ? 48 : 0;
}

/* The curve's field operations: P-256's own where the build has them, the general code
 * otherwise. `f` is the curve's field for the general code. A product of a value with itself
 * is taken as a square. */
static bool g_ec_general;                        /* tests: the general code for P-256 too */

void proven_crypto_ec_force_general_(bool on) { g_ec_general = on; }

static inline void ec_fmul(const proven_ec_curve_t *c, const proven_crypto_mp_mod_t *f, proven_crypto_limb_t *o, const proven_crypto_limb_t *a, const proven_crypto_limb_t *b) {
#if EC_P256_FIELD
    if (c->p256) { if (a == b) p256_sqr(o, a); else p256_mul(o, a, b); return; }
#endif
    (void)c;
    if (a == b) proven_crypto_mp_montsqr(f, o, a); else proven_crypto_mp_montmul(f, o, a, b);
}

static inline void ec_fadd(const proven_ec_curve_t *c, const proven_crypto_mp_mod_t *f, proven_crypto_limb_t *o, const proven_crypto_limb_t *a, const proven_crypto_limb_t *b) {
#if EC_P256_FIELD
    if (c->p256) { p256_add(o, a, b); return; }
#endif
    (void)c;
    proven_crypto_mp_add(f, o, a, b);
}

static inline void ec_fsub(const proven_ec_curve_t *c, const proven_crypto_mp_mod_t *f, proven_crypto_limb_t *o, const proven_crypto_limb_t *a, const proven_crypto_limb_t *b) {
#if EC_P256_FIELD
    if (c->p256) { p256_sub(o, a, b); return; }
#endif
    (void)c;
    proven_crypto_mp_sub(f, o, a, b);
}

/* 1 / a in the field (0 for 0), Montgomery form in and out. */
static inline void ec_finv(const proven_ec_curve_t *c, proven_crypto_limb_t *o, const proven_crypto_limb_t *a) {
#if EC_P256_FIELD
    if (c->p256) { p256_inv(o, a); return; }
#endif
    proven_crypto_mp_inv_prime(&c->p, o, a);
}

static bool ec_curve_init(proven_ec_curve_t *c, proven_crypto_ec_curve_t id) {
    const proven_byte_t *p, *n, *b, *gx, *gy;
    if (id == PROVEN_CRYPTO_EC_P256) { c->bytes = 32; p = EC_P256_P; n = EC_P256_N; b = EC_P256_B; gx = EC_P256_GX; gy = EC_P256_GY; c->gtab = &EC_P256_GTAB[0][0]; }
    else if (id == PROVEN_CRYPTO_EC_P384) { c->bytes = 48; p = EC_P384_P; n = EC_P384_N; b = EC_P384_B; gx = EC_P384_GX; gy = EC_P384_GY; c->gtab = &EC_P384_GTAB[0][0]; }
    else return false;
    c->limbs = PROVEN_CRYPTO_MP_LIMBS(c->bytes);
    c->p256 = EC_P256_FIELD && id == PROVEN_CRYPTO_EC_P256 && !g_ec_general;
    if (!proven_crypto_mp_mod_init(&c->p, p, c->bytes) || !proven_crypto_mp_mod_init(&c->n, n, c->bytes)) return false;
    proven_crypto_limb_t t[EC_LIMBS], three[EC_LIMBS], zero[EC_LIMBS];
    for (proven_size_t i = 0; i < c->limbs; ++i) { t[i] = 0; three[i] = 0; zero[i] = 0; }
    t[0] = 1; three[0] = 3;
    proven_crypto_mp_to_mont(&c->p, c->one, t);
    proven_crypto_mp_to_mont(&c->p, three, three);
    proven_crypto_mp_sub(&c->p, c->a, zero, three);
    if (!proven_crypto_mp_load_be(t, c->limbs, b, c->bytes)) return false;
    proven_crypto_mp_to_mont(&c->p, c->b, t);
    if (!proven_crypto_mp_load_be(t, c->limbs, gx, c->bytes)) return false;
    proven_crypto_mp_to_mont(&c->p, c->gx, t);
    if (!proven_crypto_mp_load_be(t, c->limbs, gy, c->bytes)) return false;
    proven_crypto_mp_to_mont(&c->p, c->gy, t);
    return true;
}

static void ec_copy(proven_crypto_limb_t *out, const proven_crypto_limb_t *a, proven_size_t limbs) {
    for (proven_size_t i = 0; i < limbs; ++i) out[i] = a[i];
}

static void ec_set_infinity(const proven_ec_curve_t *c, proven_ec_point_t *p) {
    for (proven_size_t i = 0; i < EC_LIMBS; ++i) { p->x[i] = 0; p->y[i] = 0; p->z[i] = 0; }
    ec_copy(p->y, c->one, c->limbs);
}

#define M(o, a, b) ec_fmul(c, f, o, a, b)
#define A(o, a, b) ec_fadd(c, f, o, a, b)
#define S(o, a, b) ec_fsub(c, f, o, a, b)

/* Algorithm 4 of Renes-Costello-Batina: complete addition for y^2 = x^3 - 3x + b. Twelve
 * multiplications and two by b. `out` may be either input. */
static void ec_add(const proven_ec_curve_t *c, proven_ec_point_t *out, const proven_ec_point_t *p, const proven_ec_point_t *q) {
    const proven_crypto_mp_mod_t *f = &c->p;
    proven_crypto_limb_t t0[EC_LIMBS], t1[EC_LIMBS], t2[EC_LIMBS], t3[EC_LIMBS], t4[EC_LIMBS];
    proven_crypto_limb_t x3[EC_LIMBS], y3[EC_LIMBS], z3[EC_LIMBS];
    M(t0, p->x, q->x); M(t1, p->y, q->y); M(t2, p->z, q->z);
    A(t3, p->x, p->y); A(t4, q->x, q->y); M(t3, t3, t4);
    A(t4, t0, t1); S(t3, t3, t4); A(t4, p->y, p->z);
    A(x3, q->y, q->z); M(t4, t4, x3); A(x3, t1, t2);
    S(t4, t4, x3); A(x3, p->x, p->z); A(y3, q->x, q->z);
    M(x3, x3, y3); A(y3, t0, t2); S(y3, x3, y3);
    M(z3, c->b, t2); S(x3, y3, z3); A(z3, x3, x3);
    A(x3, x3, z3); S(z3, t1, x3); A(x3, t1, x3);
    M(y3, c->b, y3); A(t1, t2, t2); A(t2, t1, t2);
    S(y3, y3, t2); S(y3, y3, t0); A(t1, y3, y3);
    A(y3, t1, y3); A(t1, t0, t0); A(t0, t1, t0);
    S(t0, t0, t2); M(t1, t4, y3); M(t2, t0, y3);
    M(y3, x3, z3); A(y3, y3, t2); M(x3, t3, x3);
    S(x3, x3, t1); M(z3, t4, z3); M(t1, t3, t0);
    A(z3, z3, t1);
    ec_copy(out->x, x3, c->limbs); ec_copy(out->y, y3, c->limbs); ec_copy(out->z, z3, c->limbs);
}

/* Algorithm 5: the same sum when the second point is given by affine coordinates (qx, qy),
 * which cannot be the point at infinity. Eleven multiplications and two by b. */
static void ec_add_affine(const proven_ec_curve_t *c, proven_ec_point_t *out, const proven_ec_point_t *p, const proven_crypto_limb_t *qx, const proven_crypto_limb_t *qy) {
    const proven_crypto_mp_mod_t *f = &c->p;
    proven_crypto_limb_t t0[EC_LIMBS], t1[EC_LIMBS], t2[EC_LIMBS], t3[EC_LIMBS], t4[EC_LIMBS];
    proven_crypto_limb_t x3[EC_LIMBS], y3[EC_LIMBS], z3[EC_LIMBS];
    M(t0, p->x, qx); M(t1, p->y, qy); A(t3, qx, qy);
    A(t4, p->x, p->y); M(t3, t3, t4); A(t4, t0, t1);
    S(t3, t3, t4); M(t4, qy, p->z); A(t4, t4, p->y);
    M(y3, qx, p->z); A(y3, y3, p->x); M(z3, c->b, p->z);
    S(x3, y3, z3); A(z3, x3, x3); A(x3, x3, z3);
    S(z3, t1, x3); A(x3, t1, x3); M(y3, c->b, y3);
    A(t1, p->z, p->z); A(t2, t1, p->z); S(y3, y3, t2);
    S(y3, y3, t0); A(t1, y3, y3); A(y3, t1, y3);
    A(t1, t0, t0); A(t0, t1, t0); S(t0, t0, t2);
    M(t1, t4, y3); M(t2, t0, y3); M(y3, x3, z3);
    A(y3, y3, t2); M(x3, t3, x3); S(x3, x3, t1);
    M(z3, t4, z3); M(t1, t3, t0); A(z3, z3, t1);
    ec_copy(out->x, x3, c->limbs); ec_copy(out->y, y3, c->limbs); ec_copy(out->z, z3, c->limbs);
}

/* Algorithm 6: complete doubling. Eight multiplications, three squarings and two by b. */
static void ec_double(const proven_ec_curve_t *c, proven_ec_point_t *out, const proven_ec_point_t *p) {
    const proven_crypto_mp_mod_t *f = &c->p;
    proven_crypto_limb_t t0[EC_LIMBS], t1[EC_LIMBS], t2[EC_LIMBS], t3[EC_LIMBS];
    proven_crypto_limb_t x3[EC_LIMBS], y3[EC_LIMBS], z3[EC_LIMBS];
    M(t0, p->x, p->x); M(t1, p->y, p->y); M(t2, p->z, p->z);
    M(t3, p->x, p->y); A(t3, t3, t3); M(z3, p->x, p->z);
    A(z3, z3, z3); M(y3, c->b, t2); S(y3, y3, z3);
    A(x3, y3, y3); A(y3, x3, y3); S(x3, t1, y3);
    A(y3, t1, y3); M(y3, x3, y3); M(x3, x3, t3);
    A(t3, t2, t2); A(t2, t2, t3); M(z3, c->b, z3);
    S(z3, z3, t2); S(z3, z3, t0); A(t3, z3, z3);
    A(z3, z3, t3); A(t3, t0, t0); A(t0, t3, t0);
    S(t0, t0, t2); M(t0, t0, z3); A(y3, y3, t0);
    M(t0, p->y, p->z); A(t0, t0, t0); M(z3, t0, z3);
    S(x3, x3, z3); M(z3, t0, t1); A(z3, z3, z3);
    A(z3, z3, z3);
    ec_copy(out->x, x3, c->limbs); ec_copy(out->y, y3, c->limbs); ec_copy(out->z, z3, c->limbs);
}
#undef M
#undef A
#undef S

/* k * p, k a reduced scalar in plain limbs (SECRET). A fixed window of four bits: sixteen
 * multiples in a table, every entry read on every lookup and one kept by mask. */
static void ec_mul(const proven_ec_curve_t *c, proven_ec_point_t *out, const proven_crypto_limb_t *k, const proven_ec_point_t *p) {
    proven_ec_point_t table[16];
    ec_set_infinity(c, &table[0]);
    table[1] = *p;
    for (int i = 2; i < 16; ++i) {
        if (i % 2 == 0) ec_double(c, &table[i], &table[i / 2]);
        else ec_add(c, &table[i], &table[i - 1], p);
    }
    proven_ec_point_t acc, pick;
    ec_set_infinity(c, &acc);
    for (proven_size_t i = c->limbs * EC_NIBBLES; i-- > 0;) {
        for (int d = 0; d < 4; ++d) ec_double(c, &acc, &acc);
        proven_crypto_limb_t nib = (k[i / EC_NIBBLES] >> (4 * (i % EC_NIBBLES))) & 15u;
        for (proven_size_t j = 0; j < EC_LIMBS; ++j) { pick.x[j] = 0; pick.y[j] = 0; pick.z[j] = 0; }
        for (proven_crypto_limb_t t = 0; t < 16; ++t) {
            proven_crypto_limb_t x = t ^ nib;
            proven_crypto_limb_t mask = ((x | ((proven_crypto_limb_t)0 - x)) >> (PROVEN_CRYPTO_MP_LIMB_BITS - 1)) - 1u;      /* all ones when t == nib */
            for (proven_size_t j = 0; j < c->limbs; ++j) {
                pick.x[j] |= table[t].x[j] & mask;
                pick.y[j] |= table[t].y[j] & mask;
                pick.z[j] |= table[t].z[j] & mask;
            }
        }
        ec_add(c, &acc, &acc, &pick);
    }
    *out = acc;
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)table, .size = sizeof table });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&pick, .size = sizeof pick });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&acc, .size = sizeof acc });
}

/* k * G for a reduced scalar k in plain limbs (SECRET), by a signed comb of five teeth.
 *
 * An odd k of L = 5d bits is the sum of s_i 2^i with every digit s_i +1 or -1: the bits t_i of
 * t = (k + 2^L - 1) / 2 = (k - 1) / 2 + 2^(L - 1) give s_i = 2 t_i - 1. Column j of the comb is
 * the digits j, j + d, ..., j + 4d; it adds the table's entry named by its four low digits
 * when its top digit is +1, and the negative of the entry named by their complement when it
 * is -1. So every column adds a point - none is zero - and the walk is d doublings and d
 * additions. An even k is replaced by n - k, which is odd, and the result negated.
 * Every entry is read on every lookup and one kept by mask; the choices of n - k and of the
 * signs are masks too. */
static void ec_mul_base(const proven_ec_curve_t *c, proven_ec_point_t *out, const proven_crypto_limb_t *k) {
    proven_crypto_limb_t table[EC_GTAB_POINTS][2][EC_LIMBS], px[EC_LIMBS], py[EC_LIMBS], ny[EC_LIMBS], t[EC_LIMBS + 1], zero[EC_LIMBS];
    proven_ec_point_t acc;
    const proven_size_t s = c->limbs, d = (c->bytes * 8 + EC_COMB_TEETH - 1) / EC_COMB_TEETH;
    const unsigned w = PROVEN_CRYPTO_MP_LIMB_BITS;
    for (proven_size_t j = 0; j < EC_GTAB_POINTS; ++j) {
        const proven_byte_t *e = c->gtab + j * 2 * c->bytes;
        (void)proven_crypto_mp_load_be(table[j][0], s, e, c->bytes);
        (void)proven_crypto_mp_load_be(table[j][1], s, e + c->bytes, c->bytes);
        proven_crypto_mp_to_mont(&c->p, table[j][0], table[j][0]);
        proven_crypto_mp_to_mont(&c->p, table[j][1], table[j][1]);
    }
    /* k, or n - k when k is even; then t = (that - 1) / 2 + 2^(L - 1), in s + 1 limbs. */
    const proven_crypto_limb_t even = (proven_crypto_limb_t)0 - ((k[0] & 1u) ^ 1u);
    for (proven_size_t i = 0; i < s; ++i) zero[i] = 0;
    (void)proven_crypto_mp_sub_raw(t, c->n.n, k, s);
    proven_crypto_mp_select(t, t, k, even, s);
    for (proven_size_t i = 0; i < s; ++i) t[i] = (t[i] >> 1) | (i + 1 < s ? t[i + 1] << (w - 1) : 0u);
    t[s] = 0;
    const proven_size_t top = EC_COMB_TEETH * d - 1;
    t[top / w] |= (proven_crypto_limb_t)1 << (top % w);
    ec_set_infinity(c, &acc);
    for (proven_size_t j = d; j-- > 0;) {
        ec_double(c, &acc, &acc);
        proven_crypto_limb_t low = 0;
        for (proven_size_t b = 0; b < EC_COMB_TEETH - 1; ++b) {
            const proven_size_t pos = j + b * d;
            low |= ((t[pos / w] >> (pos % w)) & 1u) << b;
        }
        const proven_size_t pos = j + (EC_COMB_TEETH - 1) * d;
        const proven_crypto_limb_t plus = (proven_crypto_limb_t)0 - ((t[pos / w] >> (pos % w)) & 1u);   /* all ones when the top digit is +1 */
        const proven_crypto_limb_t want = (low & plus) | ((low ^ 15u) & ~plus);
        for (proven_size_t i = 0; i < s; ++i) { px[i] = 0; py[i] = 0; }
        for (proven_crypto_limb_t e = 0; e < EC_GTAB_POINTS; ++e) {
            const proven_crypto_limb_t x = e ^ want;
            const proven_crypto_limb_t mask = ((x | ((proven_crypto_limb_t)0 - x)) >> (w - 1)) - 1u;      /* all ones when e == want */
            for (proven_size_t i = 0; i < s; ++i) {
                px[i] |= table[e][0][i] & mask;
                py[i] |= table[e][1][i] & mask;
            }
        }
        proven_crypto_mp_sub(&c->p, ny, zero, py);
        proven_crypto_mp_select(py, py, ny, plus, s);
        ec_add_affine(c, &acc, &acc, px, py);
    }
    /* The negative when k was even: Y becomes p - Y (and zero stays zero). */
    proven_crypto_mp_sub(&c->p, ny, zero, acc.y);
    proven_crypto_mp_select(acc.y, ny, acc.y, even, s);
    *out = acc;
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)t, .size = sizeof t });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)px, .size = sizeof px });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)py, .size = sizeof py });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)ny, .size = sizeof ny });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&acc, .size = sizeof acc });
}

/* Affine coordinates in plain limbs. Returns all-ones when the point is at infinity (the
 * outputs are then zero). */
static proven_crypto_limb_t ec_affine(const proven_ec_curve_t *c, proven_crypto_limb_t *x, proven_crypto_limb_t *y, const proven_ec_point_t *p) {
    proven_crypto_limb_t zi[EC_LIMBS];
    ec_finv(c, zi, p->z);
    ec_fmul(c, &c->p, x, p->x, zi);
    ec_fmul(c, &c->p, y, p->y, zi);
    proven_crypto_mp_from_mont(&c->p, x, x);
    proven_crypto_mp_from_mont(&c->p, y, y);
    proven_crypto_limb_t inf = proven_crypto_mp_is_zero(p->z, c->limbs);
    PROVEN_CT_PUBLIC(&inf, sizeof inf);          /* a result at infinity is refused, which is seen */
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)zi, .size = sizeof zi });
    return inf;
}

/* An uncompressed point from the wire. Everything about it is public. */
static bool ec_decode(const proven_ec_curve_t *c, proven_ec_point_t *out, proven_mem_view_t pub) {
    if (!pub.ptr || pub.size != 1 + 2 * c->bytes || pub.ptr[0] != 0x04) return false;
    proven_crypto_limb_t x[EC_LIMBS], y[EC_LIMBS], lhs[EC_LIMBS], rhs[EC_LIMBS];
    if (!proven_crypto_mp_load_be(x, c->limbs, pub.ptr + 1, c->bytes)) return false;
    if (!proven_crypto_mp_load_be(y, c->limbs, pub.ptr + 1 + c->bytes, c->bytes)) return false;
    if (!proven_crypto_mp_lt(x, c->p.n, c->limbs) || !proven_crypto_mp_lt(y, c->p.n, c->limbs)) return false;
    proven_crypto_mp_to_mont(&c->p, x, x);
    proven_crypto_mp_to_mont(&c->p, y, y);
    /* y^2 == x^3 + ax + b */
    ec_fmul(c, &c->p, lhs, y, y);
    ec_fmul(c, &c->p, rhs, x, x);
    ec_fadd(c, &c->p, rhs, rhs, c->a);
    ec_fmul(c, &c->p, rhs, rhs, x);
    ec_fadd(c, &c->p, rhs, rhs, c->b);
    if (!proven_crypto_mp_eq(lhs, rhs, c->limbs)) return false;
    for (proven_size_t i = 0; i < EC_LIMBS; ++i) { out->x[i] = 0; out->y[i] = 0; out->z[i] = 0; }
    ec_copy(out->x, x, c->limbs); ec_copy(out->y, y, c->limbs); ec_copy(out->z, c->one, c->limbs);
    return true;
}

/* A private scalar from bytes: in [1, n-1], or refused. The comparison's result is public (a
 * key out of range is not a key), the value is not. */
static bool ec_load_scalar(const proven_ec_curve_t *c, proven_crypto_limb_t *k, const proven_byte_t *priv) {
    if (!proven_crypto_mp_load_be(k, c->limbs, priv, c->bytes)) return false;
    proven_crypto_limb_t ok = proven_crypto_mp_lt(k, c->n.n, c->limbs) & ~proven_crypto_mp_is_zero(k, c->limbs);
    PROVEN_CT_PUBLIC(&ok, sizeof ok);
    return ok != 0;
}

static void ec_base(const proven_ec_curve_t *c, proven_ec_point_t *g) {
    for (proven_size_t i = 0; i < EC_LIMBS; ++i) { g->x[i] = 0; g->y[i] = 0; g->z[i] = 0; }
    ec_copy(g->x, c->gx, c->limbs); ec_copy(g->y, c->gy, c->limbs); ec_copy(g->z, c->one, c->limbs);
}

bool proven_crypto_ec_public(proven_crypto_ec_curve_t curve, const proven_byte_t *priv, proven_byte_t *pub_out) {
    proven_ec_curve_t c;
    proven_crypto_limb_t k[EC_LIMBS], x[EC_LIMBS], y[EC_LIMBS];
    proven_ec_point_t r;
    if (!ec_curve_init(&c, curve) || !ec_load_scalar(&c, k, priv)) return false;
    ec_mul_base(&c, &r, k);
    proven_crypto_limb_t inf = ec_affine(&c, x, y, &r);
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)k, .size = sizeof k });
    if (inf) return false;
    pub_out[0] = 0x04;
    proven_crypto_mp_store_be(pub_out + 1, c.bytes, x, c.limbs);
    proven_crypto_mp_store_be(pub_out + 1 + c.bytes, c.bytes, y, c.limbs);
    PROVEN_CT_PUBLIC(pub_out, 1 + 2 * c.bytes);
    return true;
}

bool proven_crypto_ecdh(proven_crypto_ec_curve_t curve, const proven_byte_t *priv, proven_mem_view_t peer, proven_byte_t *shared_out) {
    proven_ec_curve_t c;
    proven_crypto_limb_t k[EC_LIMBS], x[EC_LIMBS], y[EC_LIMBS];
    proven_ec_point_t q, r;
    if (!ec_curve_init(&c, curve) || !ec_decode(&c, &q, peer) || !ec_load_scalar(&c, k, priv)) return false;
    ec_mul(&c, &r, k, &q);
    /* The curves have prime order, so a valid point times a scalar in [1, n-1] is never the
     * point at infinity; the check costs nothing and is kept. */
    proven_crypto_limb_t inf = ec_affine(&c, x, y, &r);
    proven_crypto_mp_store_be(shared_out, c.bytes, x, c.limbs);
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)k, .size = sizeof k });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)x, .size = sizeof x });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)y, .size = sizeof y });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&r, .size = sizeof r });
    return inf == 0;
}

/* The digest as an integer mod n: its leftmost bytes, as many as the order has. */
static void ec_digest_scalar(const proven_ec_curve_t *c, proven_crypto_limb_t *z, proven_mem_view_t digest) {
    proven_size_t n = digest.size < c->bytes ? digest.size : c->bytes;
    proven_crypto_mp_reduce_be(&c->n, z, digest.ptr, n);
}

/* ---- Verification, in variable time ----
 *
 * A verification has no secret: the key, the digest and the signature are all public. The
 * functions named `_vartime` below branch and index on the values they are given, and are
 * correct only for that reason. They are for verification alone - `proven_crypto_ecdsa_verify`
 * and the suite's hooks are their only callers, and scripts/check-vartime.py fails the build
 * when anything else calls them.
 *
 * Points here are Jacobian (X : Y : Z), x = X / Z^2 and y = Y / Z^3, Z = 0 the point at
 * infinity, a type of their own so they cannot be passed where the complete formulas expect
 * theirs. The formulas are the usual incomplete ones (dbl-2001-b; add-2007-bl and
 * madd-2007-bl of the Explicit-Formulas Database), and the cases they get wrong - a point at
 * infinity, P + P, P + (-P) - are branched to. */

typedef struct { proven_crypto_limb_t x[EC_LIMBS], y[EC_LIMBS], z[EC_LIMBS]; } proven_ec_jac_t;

#define EC_WNAF_WIDTH 5                     /* digits odd in [-15, 15]: eight multiples of Q */
#define EC_WNAF_POINTS (1 << (EC_WNAF_WIDTH - 2))

#define M(o, a, b) ec_fmul(c, f, o, a, b)
#define A(o, a, b) ec_fadd(c, f, o, a, b)
#define S(o, a, b) ec_fsub(c, f, o, a, b)

static void ec_jac_set_infinity_vartime(const proven_ec_curve_t *c, proven_ec_jac_t *p) {
    for (proven_size_t i = 0; i < EC_LIMBS; ++i) { p->x[i] = 0; p->y[i] = 0; p->z[i] = 0; }
    ec_copy(p->x, c->one, c->limbs);
    ec_copy(p->y, c->one, c->limbs);
}

/* dbl-2001-b for a = -3: three multiplications and five squarings. Right for the point at
 * infinity too (Z stays zero). `out` may be `p`. */
static void ec_jac_double_vartime(const proven_ec_curve_t *c, proven_ec_jac_t *out, const proven_ec_jac_t *p) {
    const proven_crypto_mp_mod_t *f = &c->p;
    proven_crypto_limb_t delta[EC_LIMBS], gamma[EC_LIMBS], beta[EC_LIMBS], alpha[EC_LIMBS], t[EC_LIMBS];
    proven_crypto_limb_t x3[EC_LIMBS], y3[EC_LIMBS], z3[EC_LIMBS];
    M(delta, p->z, p->z); M(gamma, p->y, p->y); M(beta, p->x, gamma);
    S(t, p->x, delta); A(alpha, p->x, delta); M(alpha, alpha, t);
    A(t, alpha, alpha); A(alpha, t, alpha);                     /* 3 (X - delta)(X + delta) */
    A(beta, beta, beta); A(beta, beta, beta);                   /* 4 beta */
    M(x3, alpha, alpha); S(x3, x3, beta); S(x3, x3, beta);      /* alpha^2 - 8 beta */
    A(z3, p->y, p->z); M(z3, z3, z3); S(z3, z3, gamma); S(z3, z3, delta);
    S(t, beta, x3); M(y3, alpha, t);
    M(gamma, gamma, gamma); A(gamma, gamma, gamma); A(gamma, gamma, gamma); A(gamma, gamma, gamma);
    S(y3, y3, gamma);                                           /* alpha (4 beta - X3) - 8 gamma^2 */
    ec_copy(out->x, x3, c->limbs); ec_copy(out->y, y3, c->limbs); ec_copy(out->z, z3, c->limbs);
}

/* The sum's last step, from H = U2 - U1 and R = S2 - S1 (both nonzero H), U1 and S1, and the
 * new Z's factor. Shared by both additions. */
static void ec_jac_finish_vartime(const proven_ec_curve_t *c, proven_ec_jac_t *out, const proven_crypto_limb_t *h, const proven_crypto_limb_t *r_half,
                                  const proven_crypto_limb_t *u1, const proven_crypto_limb_t *s1, const proven_crypto_limb_t *z3) {
    const proven_crypto_mp_mod_t *f = &c->p;
    proven_crypto_limb_t i[EC_LIMBS], j[EC_LIMBS], r[EC_LIMBS], v[EC_LIMBS], x3[EC_LIMBS], y3[EC_LIMBS];
    A(i, h, h); M(i, i, i); M(j, h, i);                         /* I = (2H)^2, J = H I */
    A(r, r_half, r_half); M(v, u1, i);
    M(x3, r, r); S(x3, x3, j); S(x3, x3, v); S(x3, x3, v);       /* R^2 - J - 2V */
    S(v, v, x3); M(y3, r, v);
    M(j, s1, j); A(j, j, j); S(y3, y3, j);                       /* R (V - X3) - 2 S1 J */
    ec_copy(out->x, x3, c->limbs); ec_copy(out->y, y3, c->limbs); ec_copy(out->z, z3, c->limbs);
}

/* p + q. `out` may be either input. */
static void ec_jac_add_vartime(const proven_ec_curve_t *c, proven_ec_jac_t *out, const proven_ec_jac_t *p, const proven_ec_jac_t *q) {
    const proven_crypto_mp_mod_t *f = &c->p;
    if (proven_crypto_mp_is_zero(p->z, c->limbs)) { *out = *q; return; }
    if (proven_crypto_mp_is_zero(q->z, c->limbs)) { *out = *p; return; }
    proven_crypto_limb_t z1z1[EC_LIMBS], z2z2[EC_LIMBS], u1[EC_LIMBS], u2[EC_LIMBS], s1[EC_LIMBS], s2[EC_LIMBS], z3[EC_LIMBS];
    M(z1z1, p->z, p->z); M(z2z2, q->z, q->z);
    M(u1, p->x, z2z2); M(u2, q->x, z1z1);
    M(s1, p->y, q->z); M(s1, s1, z2z2); M(s2, q->y, p->z); M(s2, s2, z1z1);
    S(u2, u2, u1); S(s2, s2, s1);                               /* H, and R / 2 */
    if (proven_crypto_mp_is_zero(u2, c->limbs)) {
        if (proven_crypto_mp_is_zero(s2, c->limbs)) ec_jac_double_vartime(c, out, p);
        else ec_jac_set_infinity_vartime(c, out);
        return;
    }
    A(z3, p->z, q->z); M(z3, z3, z3); S(z3, z3, z1z1); S(z3, z3, z2z2); M(z3, z3, u2);
    ec_jac_finish_vartime(c, out, u2, s2, u1, s1, z3);
}

/* p + (qx, qy), the second point affine (and so not at infinity). `out` may be `p`. */
static void ec_jac_add_affine_vartime(const proven_ec_curve_t *c, proven_ec_jac_t *out, const proven_ec_jac_t *p, const proven_crypto_limb_t *qx, const proven_crypto_limb_t *qy) {
    const proven_crypto_mp_mod_t *f = &c->p;
    if (proven_crypto_mp_is_zero(p->z, c->limbs)) {
        ec_copy(out->x, qx, c->limbs); ec_copy(out->y, qy, c->limbs); ec_copy(out->z, c->one, c->limbs);
        return;
    }
    proven_crypto_limb_t z1z1[EC_LIMBS], u2[EC_LIMBS], s2[EC_LIMBS], z3[EC_LIMBS], x1[EC_LIMBS], y1[EC_LIMBS];
    M(z1z1, p->z, p->z); M(u2, qx, z1z1);
    M(s2, qy, p->z); M(s2, s2, z1z1);
    S(u2, u2, p->x); S(s2, s2, p->y);                           /* H, and R / 2 */
    if (proven_crypto_mp_is_zero(u2, c->limbs)) {
        if (proven_crypto_mp_is_zero(s2, c->limbs)) ec_jac_double_vartime(c, out, p);
        else ec_jac_set_infinity_vartime(c, out);
        return;
    }
    A(z3, p->z, u2); M(z3, z3, z3); S(z3, z3, z1z1);
    M(x1, u2, u2); S(z3, z3, x1);                               /* (Z1 + H)^2 - Z1Z1 - H^2 = 2 Z1 H */
    ec_copy(x1, p->x, c->limbs); ec_copy(y1, p->y, c->limbs);
    ec_jac_finish_vartime(c, out, u2, s2, x1, y1, z3);
}
#undef M
#undef A
#undef S

/* The width-5 NAF of k (plain limbs, below 2^bits): digit i odd in [-15, 15] or zero, any
 * two nonzero digits at least five apart. bits + 1 digits; returns the number used (one past
 * the highest nonzero digit, 0 for k = 0). */
static proven_size_t ec_wnaf_vartime(signed char *digits, const proven_crypto_limb_t *k, proven_size_t limbs, proven_size_t bits) {
    const unsigned w = PROVEN_CRYPTO_MP_LIMB_BITS;
    proven_size_t used = 0;
    unsigned carry = 0;
    for (proven_size_t i = 0; i <= bits; ++i) digits[i] = 0;
    for (proven_size_t bit = 0; bit <= bits;) {
        unsigned b = bit / w < limbs ? (unsigned)(k[bit / w] >> (bit % w)) & 1u : 0u;
        if (b == carry) { ++bit; continue; }
        unsigned word = 0;
        for (unsigned j = 0; j < EC_WNAF_WIDTH; ++j) {
            const proven_size_t at = bit + j;
            if (at / w < limbs) word |= (unsigned)((k[at / w] >> (at % w)) & 1u) << j;
        }
        word += carry;                                          /* the window's value, odd */
        carry = (word >> (EC_WNAF_WIDTH - 1)) & 1u;
        digits[bit] = (signed char)((int)word - (int)(carry << EC_WNAF_WIDTH));
        used = bit + 1;
        bit += EC_WNAF_WIDTH;
    }
    return used;
}

/* u1 G + u2 Q for public u1, u2 below the order and a public Q (affine, from ec_decode): one
 * walk of the doublings for both. Q's part reads its width-5 NAF against eight odd multiples
 * of Q; G's part is the signed comb of ec_mul_base, whose column j is added when the walk is
 * at j. */
static void ec_mul2_vartime(const proven_ec_curve_t *c, proven_ec_jac_t *out, const proven_crypto_limb_t *u1, const proven_crypto_limb_t *u2, const proven_ec_point_t *q) {
    const proven_crypto_mp_mod_t *f = &c->p;
    const proven_size_t s = c->limbs, bits = c->bytes * 8, d = (bits + EC_COMB_TEETH - 1) / EC_COMB_TEETH;
    const unsigned w = PROVEN_CRYPTO_MP_LIMB_BITS;
    proven_ec_jac_t table[EC_WNAF_POINTS], q2, acc;
    signed char naf[PROVEN_CRYPTO_EC_MAX_BYTES * 8 + 1];
    proven_crypto_limb_t t[EC_LIMBS + 1], zero[EC_LIMBS], ny[EC_LIMBS];

    /* Q, 3Q, ..., 15Q */
    const proven_size_t qtop = ec_wnaf_vartime(naf, u2, s, bits);
    if (qtop > 0) {
        for (proven_size_t i = 0; i < EC_LIMBS; ++i) { table[0].x[i] = q->x[i]; table[0].y[i] = q->y[i]; table[0].z[i] = q->z[i]; }
        ec_jac_double_vartime(c, &q2, &table[0]);
        for (proven_size_t i = 1; i < EC_WNAF_POINTS; ++i) ec_jac_add_vartime(c, &table[i], &table[i - 1], &q2);
    }
    /* G: k = u1, or n - u1 when u1 is even (and the comb's points negated); then the comb's
     * digits t = (k - 1) / 2 + 2^(L - 1). u1 = 0 leaves G out. */
    const bool gpart = !proven_crypto_mp_is_zero(u1, s), negate = (u1[0] & 1u) == 0;
    if (gpart) {
        if (negate) (void)proven_crypto_mp_sub_raw(t, c->n.n, u1, s);
        else ec_copy(t, u1, s);
        for (proven_size_t i = 0; i < s; ++i) t[i] = (t[i] >> 1) | (i + 1 < s ? t[i + 1] << (w - 1) : 0u);
        t[s] = 0;
        const proven_size_t top = EC_COMB_TEETH * d - 1;
        t[top / w] |= (proven_crypto_limb_t)1 << (top % w);
    }
    for (proven_size_t i = 0; i < s; ++i) zero[i] = 0;

    ec_jac_set_infinity_vartime(c, &acc);
    const proven_size_t start = qtop > (gpart ? d : 0) ? qtop : (gpart ? d : 0);
    for (proven_size_t i = start; i-- > 0;) {
        if (!proven_crypto_mp_is_zero(acc.z, s)) ec_jac_double_vartime(c, &acc, &acc);
        if (gpart && i < d) {
            unsigned low = 0;
            for (proven_size_t b = 0; b < EC_COMB_TEETH - 1; ++b) {
                const proven_size_t pos = i + b * d;
                low |= (unsigned)((t[pos / w] >> (pos % w)) & 1u) << b;
            }
            const proven_size_t pos = i + (EC_COMB_TEETH - 1) * d;
            const bool plus = ((t[pos / w] >> (pos % w)) & 1u) != 0;
            const proven_byte_t *e = c->gtab + (plus ? low : low ^ 15u) * 2 * c->bytes;
            proven_crypto_limb_t px[EC_LIMBS], py[EC_LIMBS];
            (void)proven_crypto_mp_load_be(px, s, e, c->bytes);
            (void)proven_crypto_mp_load_be(py, s, e + c->bytes, c->bytes);
            proven_crypto_mp_to_mont(f, px, px);
            proven_crypto_mp_to_mont(f, py, py);
            if (plus == negate) { proven_crypto_mp_sub(f, ny, zero, py); ec_copy(py, ny, s); }
            ec_jac_add_affine_vartime(c, &acc, &acc, px, py);
        }
        if (i < qtop && naf[i] != 0) {
            const int dg = naf[i];
            if (dg > 0) ec_jac_add_vartime(c, &acc, &acc, &table[dg / 2]);
            else {
                proven_ec_jac_t neg = table[-dg / 2];
                proven_crypto_mp_sub(f, neg.y, zero, neg.y);
                ec_jac_add_vartime(c, &acc, &acc, &neg);
            }
        }
    }
    *out = acc;
}

/* Whether the x of a Jacobian point, reduced mod n, is r (plain, in [1, n-1]): X = r' Z^2 for
 * r' = r or, when it is below p, r + n. No inversion. False for the point at infinity. */
static bool ec_jac_x_is_vartime(const proven_ec_curve_t *c, const proven_ec_jac_t *p, const proven_crypto_limb_t *r) {
    const proven_crypto_mp_mod_t *f = &c->p;
    proven_crypto_limb_t zz[EC_LIMBS], rm[EC_LIMBS], t[EC_LIMBS];
    if (proven_crypto_mp_is_zero(p->z, c->limbs)) return false;
    proven_crypto_mp_montmul(f, zz, p->z, p->z);
    proven_crypto_mp_to_mont(f, rm, r);
    proven_crypto_mp_montmul(f, t, rm, zz);
    if (proven_crypto_mp_eq(t, p->x, c->limbs)) return true;
    if (proven_crypto_mp_add_raw(rm, r, c->n.n, c->limbs) != 0 || !proven_crypto_mp_lt(rm, f->n, c->limbs)) return false;
    proven_crypto_mp_to_mont(f, rm, rm);
    proven_crypto_mp_montmul(f, t, rm, zz);
    return proven_crypto_mp_eq(t, p->x, c->limbs) != 0;
}

bool proven_crypto_ecdsa_verify(proven_crypto_ec_curve_t curve, proven_mem_view_t pub, proven_mem_view_t digest,
                                proven_mem_view_t r_in, proven_mem_view_t s_in) {
    proven_ec_curve_t c;
    proven_ec_point_t q;
    proven_ec_jac_t p;
    proven_crypto_limb_t r[EC_LIMBS], s[EC_LIMBS], z[EC_LIMBS], w[EC_LIMBS], u1[EC_LIMBS], u2[EC_LIMBS];
    if (!ec_curve_init(&c, curve) || !ec_decode(&c, &q, pub)) return false;
    if (!digest.ptr || digest.size == 0 || !r_in.ptr || !s_in.ptr) return false;
    if (!proven_crypto_mp_load_be(r, c.limbs, r_in.ptr, r_in.size) || !proven_crypto_mp_load_be(s, c.limbs, s_in.ptr, s_in.size)) return false;
    if (proven_crypto_mp_is_zero(r, c.limbs) || proven_crypto_mp_is_zero(s, c.limbs)) return false;
    if (!proven_crypto_mp_lt(r, c.n.n, c.limbs) || !proven_crypto_mp_lt(s, c.n.n, c.limbs)) return false;
    ec_digest_scalar(&c, z, digest);
    /* w = s^-1; u1 = z w; u2 = r w (all mod n). */
    proven_crypto_mp_to_mont(&c.n, w, s);
    proven_crypto_mp_inv_prime(&c.n, w, w);
    proven_crypto_mp_montmul(&c.n, u1, z, w);
    proven_crypto_mp_montmul(&c.n, u2, r, w);
    ec_mul2_vartime(&c, &p, u1, u2, &q);
    return ec_jac_x_is_vartime(&c, &p, r);
}

/* One DER INTEGER of fewer than 128 content bytes: non-negative and minimal. */
static bool ec_der_int(const proven_byte_t **p, const proven_byte_t *end, proven_mem_view_t *out) {
    if (end - *p < 2 || (*p)[0] != 0x02) return false;
    proven_size_t len = (*p)[1];
    if (len == 0 || len > 127 || (proven_size_t)(end - *p - 2) < len) return false;
    const proven_byte_t *v = *p + 2;
    if (v[0] & 0x80) return false;
    if (len > 1 && v[0] == 0 && (v[1] & 0x80) == 0) return false;
    out->ptr = v; out->size = len;
    *p = v + len;
    return true;
}

bool proven_crypto_ecdsa_verify_der(proven_crypto_ec_curve_t curve, proven_mem_view_t pub, proven_mem_view_t digest,
                                    proven_mem_view_t sig) {
    if (!sig.ptr || sig.size < 8 || sig.ptr[0] != 0x30) return false;
    const proven_byte_t *p = sig.ptr + 2, *end = sig.ptr + sig.size;
    proven_size_t len = sig.ptr[1];
    if (len == 0x81) {
        /* The long form is right only for 128 bytes and more. */
        if (sig.size < 3 || sig.ptr[2] < 0x80) return false;
        len = sig.ptr[2];
        p = sig.ptr + 3;
    } else if (len >= 0x80) {
        return false;
    }
    if ((proven_size_t)(end - p) != len) return false;
    proven_mem_view_t r, s;
    if (!ec_der_int(&p, end, &r) || !ec_der_int(&p, end, &s) || p != end) return false;
    return proven_crypto_ecdsa_verify(curve, pub, digest, r, s);
}

/* RFC 6979 section 3.2: the nonce from the key and the digest. */
typedef struct {
    proven_hmac_hash_t hash;
    proven_size_t hlen;
    proven_byte_t k[PROVEN_HMAC_MAX_SIZE];
    proven_byte_t v[PROVEN_HMAC_MAX_SIZE];
} proven_ec_drbg_t;

static bool ec_drbg_step(proven_ec_drbg_t *d, int sep, const proven_byte_t *x, const proven_byte_t *h, proven_size_t n) {
    proven_hmac_t m;
    if (proven_hmac_init(&m, d->hash, (proven_mem_view_t){ .ptr = d->k, .size = d->hlen }) != PROVEN_OK) return false;
    proven_hmac_update(&m, (proven_mem_view_t){ .ptr = d->v, .size = d->hlen });
    if (sep >= 0) {
        proven_byte_t b = (proven_byte_t)sep;
        proven_hmac_update(&m, (proven_mem_view_t){ .ptr = &b, .size = 1 });
        if (x) {
            proven_hmac_update(&m, (proven_mem_view_t){ .ptr = x, .size = n });
            proven_hmac_update(&m, (proven_mem_view_t){ .ptr = h, .size = n });
        }
        proven_hmac_final(&m, d->k);
        if (proven_hmac(d->hash, (proven_mem_view_t){ .ptr = d->k, .size = d->hlen }, (proven_mem_view_t){ .ptr = d->v, .size = d->hlen }, d->v) != PROVEN_OK) return false;
    } else {
        proven_hmac_final(&m, d->v);
    }
    return true;
}

bool proven_crypto_ecdsa_sign(proven_crypto_ec_curve_t curve, int hmac_hash, const proven_byte_t *priv,
                              proven_mem_view_t digest, proven_byte_t *sig_out) {
    proven_ec_curve_t c;
    proven_ec_drbg_t d;
    proven_ec_point_t rp;
    proven_crypto_limb_t key[EC_LIMBS], z[EC_LIMBS], k[EC_LIMBS], r[EC_LIMBS], s[EC_LIMBS], t[EC_LIMBS], x[EC_LIMBS], y[EC_LIMBS];
    proven_byte_t zb[PROVEN_CRYPTO_EC_MAX_BYTES], kb[PROVEN_CRYPTO_EC_MAX_BYTES + PROVEN_HMAC_MAX_SIZE], xb[PROVEN_CRYPTO_EC_MAX_BYTES];
    bool ok = false;
    d.hash = (proven_hmac_hash_t)hmac_hash;
    d.hlen = proven_hmac_size(d.hash);
    if (d.hlen == 0 || !digest.ptr || digest.size == 0) return false;
    if (!ec_curve_init(&c, curve) || !ec_load_scalar(&c, key, priv)) return false;
    ec_digest_scalar(&c, z, digest);
    proven_crypto_mp_store_be(zb, c.bytes, z, c.limbs);
    for (proven_size_t i = 0; i < d.hlen; ++i) { d.v[i] = 0x01; d.k[i] = 0x00; }
    if (!ec_drbg_step(&d, 0x00, priv, zb, c.bytes) || !ec_drbg_step(&d, 0x01, priv, zb, c.bytes)) goto done;
    for (int tries = 0; tries < 64 && !ok; ++tries) {
        proven_size_t have = 0;
        while (have < c.bytes) {
            if (!ec_drbg_step(&d, -1, NULL, NULL, 0)) goto done;
            for (proven_size_t i = 0; i < d.hlen; ++i) kb[have + i] = d.v[i];
            have += d.hlen;
        }
        /* Whether a candidate is in range is not a secret: a refused one is thrown away. */
        if (ec_load_scalar(&c, k, kb)) {
            ec_mul_base(&c, &rp, k);
            if (!ec_affine(&c, x, y, &rp)) {
                proven_crypto_mp_store_be(xb, c.bytes, x, c.limbs);
                proven_crypto_mp_reduce_be(&c.n, r, xb, c.bytes);
                PROVEN_CT_PUBLIC(r, sizeof r);    /* r is half of the signature */
                /* s = k^-1 (z + r d) mod n */
                proven_crypto_mp_to_mont(&c.n, t, key);
                proven_crypto_mp_montmul(&c.n, t, t, r);                 /* r d */
                proven_crypto_mp_add(&c.n, t, t, z);
                proven_crypto_mp_to_mont(&c.n, k, k);
                proven_crypto_mp_inv_prime(&c.n, k, k);                   /* k^-1, Montgomery form */
                proven_crypto_mp_montmul(&c.n, s, t, k);
                PROVEN_CT_PUBLIC(s, sizeof s);
                if (!proven_crypto_mp_is_zero(r, c.limbs) && !proven_crypto_mp_is_zero(s, c.limbs)) {
                    proven_crypto_mp_store_be(sig_out, c.bytes, r, c.limbs);
                    proven_crypto_mp_store_be(sig_out + c.bytes, c.bytes, s, c.limbs);
                    ok = true;
                }
            }
        }
        if (!ok && !ec_drbg_step(&d, 0x00, NULL, NULL, 0)) goto done;
    }
done:
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&d, .size = sizeof d });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)key, .size = sizeof key });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)k, .size = sizeof k });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)t, .size = sizeof t });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = kb, .size = sizeof kb });
    proven_mem_wipe((proven_mem_mut_t){ .ptr = (proven_byte_t *)&rp, .size = sizeof rp });
    return ok;
}

/* ---- for the suite ---- */

/* Every entry of the generator's table against the general code: entry i must be the walk's
 * m * G for m = 2^(4d) + the sum over b < 4 of (+1 or -1, by bit b of i) 2^(b d), G being the
 * curve's own constant. */
bool proven_crypto_ec_test_generator_table(proven_crypto_ec_curve_t curve) {
    proven_ec_curve_t c;
    proven_ec_point_t g, r;
    proven_crypto_limb_t k[EC_LIMBS], plus[EC_LIMBS], minus[EC_LIMBS], x[EC_LIMBS], y[EC_LIMBS];
    proven_byte_t got[2 * PROVEN_CRYPTO_EC_MAX_BYTES];
    if (!ec_curve_init(&c, curve)) return false;
    const proven_size_t d = (c.bytes * 8 + EC_COMB_TEETH - 1) / EC_COMB_TEETH, w = PROVEN_CRYPTO_MP_LIMB_BITS;
    ec_base(&c, &g);
    for (proven_size_t i = 0; i < EC_GTAB_POINTS; ++i) {
        for (proven_size_t j = 0; j < EC_LIMBS; ++j) { plus[j] = 0; minus[j] = 0; }
        plus[(4 * d) / w] |= (proven_crypto_limb_t)1 << ((4 * d) % w);
        for (proven_size_t b = 0; b < 4; ++b) {
            proven_crypto_limb_t *to = ((i >> b) & 1u) ? plus : minus;
            to[(b * d) / w] |= (proven_crypto_limb_t)1 << ((b * d) % w);
        }
        (void)proven_crypto_mp_sub_raw(k, plus, minus, c.limbs);       /* positive: 2^(4d) is larger than the rest */
        ec_mul(&c, &r, k, &g);
        if (ec_affine(&c, x, y, &r)) return false;
        proven_crypto_mp_store_be(got, c.bytes, x, c.limbs);
        proven_crypto_mp_store_be(got + c.bytes, c.bytes, y, c.limbs);
        const proven_byte_t *want = c.gtab + i * 2 * c.bytes;
        for (proven_size_t j = 0; j < 2 * c.bytes; ++j) if (got[j] != want[j]) return false;
    }
    return true;
}

int proven_crypto_ec_test_p256_reduce(const proven_u64 t[8]) {
#if EC_P256_FIELD
    proven_ec_curve_t c;
    if (!ec_curve_init(&c, PROVEN_CRYPTO_EC_P256)) return -1;
    proven_crypto_limb_t lo[4], hi[4], one[4] = { 1, 0, 0, 0 }, x[4], want[4], got[4];
    for (int i = 0; i < 4; ++i) { lo[i] = t[i]; hi[i] = t[4 + i]; }
    if (!proven_crypto_mp_lt(hi, c.p.n, 4)) return -1;
    /* t / R = hi + lo / R (mod p): lo brought below p (it is below 2p), then lo R^-1 by the
     * general Montgomery product with 1, then the general modular addition. */
    if (!proven_crypto_mp_lt(lo, c.p.n, 4)) {
        proven_u64 borrow = 0;
        for (int i = 0; i < 4; ++i) {
            const ec_u128 v = (ec_u128)lo[i] - P256_P[i] - borrow;
            lo[i] = (proven_u64)v;
            borrow = (proven_u64)(v >> 64) & 1u;
        }
    }
    proven_crypto_mp_montmul(&c.p, x, lo, one);
    proven_crypto_mp_add(&c.p, want, hi, x);
    p256_reduce8(got, t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]);
    return proven_crypto_mp_eq(got, want, 4) != 0;
#else
    (void)t;
    return -1;
#endif
}

int proven_crypto_ec_test_p256_addsub(const proven_u64 a[4], const proven_u64 b[4]) {
#if EC_P256_FIELD
    proven_ec_curve_t c;
    if (!ec_curve_init(&c, PROVEN_CRYPTO_EC_P256)) return -1;
    proven_crypto_limb_t x[4], y[4], want[4], got[4];
    for (int i = 0; i < 4; ++i) { x[i] = a[i]; y[i] = b[i]; }
    if (!proven_crypto_mp_lt(x, c.p.n, 4) || !proven_crypto_mp_lt(y, c.p.n, 4)) return -1;
    proven_crypto_mp_add(&c.p, want, x, y);
    p256_add(got, x, y);
    if (!proven_crypto_mp_eq(got, want, 4)) return 0;
    proven_crypto_mp_sub(&c.p, want, x, y);
    p256_sub(got, x, y);
    return proven_crypto_mp_eq(got, want, 4) != 0;
#else
    (void)a; (void)b;
    return -1;
#endif
}

/* k * G by the table and by the general walk, for any k below the order - zero too, which no
 * key or nonce is. True when the two agree: both the point at infinity, or the same point. */
bool proven_crypto_ec_test_base_mul(proven_crypto_ec_curve_t curve, const proven_byte_t *k_be) {
    proven_ec_curve_t c;
    proven_ec_point_t g, a, b;
    proven_crypto_limb_t k[EC_LIMBS], ax[EC_LIMBS], ay[EC_LIMBS], bx[EC_LIMBS], by[EC_LIMBS];
    if (!ec_curve_init(&c, curve) || !proven_crypto_mp_load_be(k, c.limbs, k_be, c.bytes) || !proven_crypto_mp_lt(k, c.n.n, c.limbs)) return false;
    ec_base(&c, &g);
    ec_mul(&c, &a, k, &g);
    ec_mul_base(&c, &b, k);
    const bool ainf = ec_affine(&c, ax, ay, &a) != 0, binf = ec_affine(&c, bx, by, &b) != 0;
    if (ainf || binf) return ainf && binf;
    return proven_crypto_mp_eq(ax, bx, c.limbs) != 0 && proven_crypto_mp_eq(ay, by, c.limbs) != 0;
}

/* Whether a Jacobian point and a projective one are the same point (both at infinity, or
 * equal affine coordinates). */
static bool ec_test_same_vartime(const proven_ec_curve_t *c, const proven_ec_jac_t *j, const proven_ec_point_t *p) {
    proven_crypto_limb_t zi[EC_LIMBS], z2[EC_LIMBS], jx[EC_LIMBS], jy[EC_LIMBS], px[EC_LIMBS], py[EC_LIMBS];
    const bool pinf = ec_affine(c, px, py, p) != 0, jinf = proven_crypto_mp_is_zero(j->z, c->limbs) != 0;
    if (pinf || jinf) return pinf && jinf;
    proven_crypto_mp_inv_prime(&c->p, zi, j->z);
    proven_crypto_mp_montmul(&c->p, z2, zi, zi);
    proven_crypto_mp_montmul(&c->p, jx, j->x, z2);
    proven_crypto_mp_montmul(&c->p, z2, z2, zi);
    proven_crypto_mp_montmul(&c->p, jy, j->y, z2);
    proven_crypto_mp_from_mont(&c->p, jx, jx);
    proven_crypto_mp_from_mont(&c->p, jy, jy);
    return proven_crypto_mp_eq(jx, px, c->limbs) != 0 && proven_crypto_mp_eq(jy, py, c->limbs) != 0;
}

/* A projective point as a Jacobian one with Z = 1, by way of its affine coordinates. */
static void ec_test_to_jac_vartime(const proven_ec_curve_t *c, proven_ec_jac_t *j, const proven_ec_point_t *p) {
    proven_crypto_limb_t x[EC_LIMBS], y[EC_LIMBS];
    ec_jac_set_infinity_vartime(c, j);
    if (ec_affine(c, x, y, p)) return;
    proven_crypto_mp_to_mont(&c->p, j->x, x);
    proven_crypto_mp_to_mont(&c->p, j->y, y);
    ec_copy(j->z, c->one, c->limbs);
}

/* k1 G + k2 Q by verification's walk and by the constant-time code (the comb, the window, the
 * complete addition), for k1, k2 below the order and Q an encoded point. True when the two
 * agree. */
bool proven_crypto_ec_test_mul2(proven_crypto_ec_curve_t curve, const proven_byte_t *k1_be, const proven_byte_t *k2_be, proven_mem_view_t pub) {
    proven_ec_curve_t c;
    proven_ec_point_t q, a, b;
    proven_ec_jac_t v;
    proven_crypto_limb_t k1[EC_LIMBS], k2[EC_LIMBS];
    if (!ec_curve_init(&c, curve) || !ec_decode(&c, &q, pub)) return false;
    if (!proven_crypto_mp_load_be(k1, c.limbs, k1_be, c.bytes) || !proven_crypto_mp_lt(k1, c.n.n, c.limbs)) return false;
    if (!proven_crypto_mp_load_be(k2, c.limbs, k2_be, c.bytes) || !proven_crypto_mp_lt(k2, c.n.n, c.limbs)) return false;
    ec_mul_base(&c, &a, k1);
    ec_mul(&c, &b, k2, &q);
    ec_add(&c, &a, &a, &b);
    ec_mul2_vartime(&c, &v, k1, k2, &q);
    return ec_test_same_vartime(&c, &v, &a);
}

/* The cases the Jacobian formulas get wrong and branch to, against the complete formulas, for
 * P = k G (k in [1, n-1]): P + P, P + (-P), P + infinity, infinity + P, P + 2P, and twice
 * infinity - each by the projective-input addition and by the affine-input one. */
bool proven_crypto_ec_test_vartime_add(proven_crypto_ec_curve_t curve, const proven_byte_t *k_be) {
    proven_ec_curve_t c;
    proven_ec_point_t g, p, np, p2, inf, want;
    proven_ec_jac_t jp, jnp, jp2, jinf, got;
    proven_crypto_limb_t k[EC_LIMBS], zero[EC_LIMBS], px[EC_LIMBS], py[EC_LIMBS], nx[EC_LIMBS], ny[EC_LIMBS];
    if (!ec_curve_init(&c, curve) || !ec_load_scalar(&c, k, k_be)) return false;
    for (proven_size_t i = 0; i < EC_LIMBS; ++i) zero[i] = 0;
    ec_base(&c, &g);
    ec_mul(&c, &p, k, &g);
    ec_double(&c, &p2, &p);
    np = p;
    proven_crypto_mp_sub(&c.p, np.y, zero, np.y);
    ec_set_infinity(&c, &inf);
    ec_test_to_jac_vartime(&c, &jp, &p); ec_test_to_jac_vartime(&c, &jnp, &np); ec_test_to_jac_vartime(&c, &jp2, &p2);
    ec_jac_set_infinity_vartime(&c, &jinf);
    /* Affine forms of P and -P, Montgomery, for the affine-input addition. */
    if (ec_affine(&c, px, py, &p)) return false;
    proven_crypto_mp_to_mont(&c.p, px, px); proven_crypto_mp_to_mont(&c.p, py, py);
    ec_copy(nx, px, c.limbs);
    proven_crypto_mp_sub(&c.p, ny, zero, py);

    ec_add(&c, &want, &p, &p);
    ec_jac_add_vartime(&c, &got, &jp, &jp);          if (!ec_test_same_vartime(&c, &got, &want)) return false;
    ec_jac_add_affine_vartime(&c, &got, &jp, px, py); if (!ec_test_same_vartime(&c, &got, &want)) return false;
    ec_jac_double_vartime(&c, &got, &jp);             if (!ec_test_same_vartime(&c, &got, &want)) return false;
    ec_add(&c, &want, &p, &np);
    ec_jac_add_vartime(&c, &got, &jp, &jnp);          if (!ec_test_same_vartime(&c, &got, &want)) return false;
    ec_jac_add_affine_vartime(&c, &got, &jp, nx, ny); if (!ec_test_same_vartime(&c, &got, &want)) return false;
    ec_jac_add_vartime(&c, &got, &jp, &jinf);         if (!ec_test_same_vartime(&c, &got, &p)) return false;
    ec_jac_add_vartime(&c, &got, &jinf, &jp);         if (!ec_test_same_vartime(&c, &got, &p)) return false;
    ec_jac_add_affine_vartime(&c, &got, &jinf, px, py); if (!ec_test_same_vartime(&c, &got, &p)) return false;
    ec_jac_double_vartime(&c, &got, &jinf);           if (!ec_test_same_vartime(&c, &got, &inf)) return false;
    ec_add(&c, &want, &p, &p2);
    ec_jac_add_vartime(&c, &got, &jp2, &jp);          if (!ec_test_same_vartime(&c, &got, &want)) return false;
    ec_jac_add_affine_vartime(&c, &got, &jp2, px, py); if (!ec_test_same_vartime(&c, &got, &want)) return false;
    /* And a sum whose inputs are not affine: 2P + (2P + P) = 5P against the complete formulas. */
    proven_ec_jac_t j3;
    ec_jac_add_affine_vartime(&c, &j3, &jp2, px, py);
    ec_jac_double_vartime(&c, &jp2, &jp);             /* 2P with a Z that is not one */
    ec_jac_add_vartime(&c, &got, &jp2, &j3);
    ec_add(&c, &want, &want, &p2);
    return ec_test_same_vartime(&c, &got, &want);
}
