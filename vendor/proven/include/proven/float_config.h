#ifndef PROVEN_FLOAT_CONFIG_H
#define PROVEN_FLOAT_CONFIG_H

/*
 * Compile-time configuration for the float module scaffold.
 *
 * PROVEN_FMT_SHORTEST keeps the shortest-formatting mode available to the
 * policy API. The hosted default remains fixed-6 unless callers explicitly
 * request shortest mode.
 * PROVEN_ENABLE_FLOAT_FORMAT_RYU remains reserved for future table-backed
 * variants.
 */
#ifndef PROVEN_FMT_SHORTEST
#define PROVEN_FMT_SHORTEST 0
#endif

#ifndef PROVEN_ENABLE_FLOAT_FORMAT_RYU
#define PROVEN_ENABLE_FLOAT_FORMAT_RYU 0
#endif

/*
 * PROVEN_FLOAT_BIGINT_LIMBS sets the capacity, in 64-bit limbs, of the internal
 * big integer used by the exact decimal-to-binary64 fallback and by the bigint
 * division helper (proven_float_bigint_divmod_u64). It is the dominant factor in
 * their stack footprint: each big integer is 8 * LIMBS bytes, and a division
 * allocates several big integers plus 32-bit scratch.
 *
 * The hosted default of 160 limbs supports the full 767-significant-digit worst
 * case for correct binary64 rounding with margin. Embedded targets that want a
 * smaller stack can lower this (e.g. -DPROVEN_FLOAT_BIGINT_LIMBS=48). The Clinger
 * and Eisel-Lemire fast paths never touch the big integer, so short inputs are
 * unaffected; only the exact fallback for long-mantissa inputs is bounded.
 */
#ifndef PROVEN_FLOAT_BIGINT_LIMBS
#define PROVEN_FLOAT_BIGINT_LIMBS 160u
#endif

/*
 * Limbs reserved above the stored significand for scaling the exact comparison
 * by powers of five and by binary shifts. The exact-compare working values reach
 * the significand size plus roughly twenty limbs (cached 5^q up to ~13 limbs and
 * a binary shift up to ~5 limbs); this leaves comfortable margin.
 */
#ifndef PROVEN_FLOAT_BIGINT_SCALE_HEADROOM
#define PROVEN_FLOAT_BIGINT_SCALE_HEADROOM 32u
#endif

#if PROVEN_FLOAT_BIGINT_LIMBS <= PROVEN_FLOAT_BIGINT_SCALE_HEADROOM
#error "PROVEN_FLOAT_BIGINT_LIMBS must exceed PROVEN_FLOAT_BIGINT_SCALE_HEADROOM"
#endif

/*
 * PROVEN_FLOAT_MAX_SIGNIFICAND_DIGITS bounds how many significant digits the
 * exact significand keeps; digits past it become a sticky bit. It is derived so
 * the stored significand plus scaling headroom always fits the configured limb
 * capacity, using a conservative 19 significant digits per 64-bit limb
 * (64 / log2(10) ~= 19.27). Capped at 800 (>= the 767-digit binary64 worst case)
 * for the default build. Builds that derive a cap below 768 stay within one ULP
 * but give up exact rounding for inputs that need more digits than the cap.
 */
#ifndef PROVEN_FLOAT_MAX_SIGNIFICAND_DIGITS
#if ((PROVEN_FLOAT_BIGINT_LIMBS - PROVEN_FLOAT_BIGINT_SCALE_HEADROOM) * 19u) < 800u
#define PROVEN_FLOAT_MAX_SIGNIFICAND_DIGITS \
    ((PROVEN_FLOAT_BIGINT_LIMBS - PROVEN_FLOAT_BIGINT_SCALE_HEADROOM) * 19u)
#else
#define PROVEN_FLOAT_MAX_SIGNIFICAND_DIGITS 800u
#endif
#endif

#endif /* PROVEN_FLOAT_CONFIG_H */
