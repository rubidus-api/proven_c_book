#ifndef PROVEN_FLOAT_FORMAT_H
#define PROVEN_FLOAT_FORMAT_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/float_config.h"

/*
 * Policy API for the float-format module scaffold.
 *
 * The DEFAULT and SIMPLE policies intentionally preserve the current hosted
 * fixed-precision formatter behavior. The RYU policy uses the shortest-mode
 * backend when requested with PROVEN_FLOAT_FORMAT_MODE_SHORTEST.
 */
typedef enum {
    PROVEN_FLOAT_FORMAT_POLICY_DEFAULT = 0,
    PROVEN_FLOAT_FORMAT_POLICY_SIMPLE = 1,
    PROVEN_FLOAT_FORMAT_POLICY_RYU = 2,
} proven_float_format_policy_t;

typedef enum {
    PROVEN_FLOAT_FORMAT_MODE_FIXED = 0,
    PROVEN_FLOAT_FORMAT_MODE_SHORTEST = 1,
    /**
     * @brief Always scientific: "d.ddde+/-XX", `precision` digits after the point, whatever
     *        the magnitude. This is printf's %e - the one float form FIXED and SHORTEST do
     *        not cover, because FIXED only reaches scientific for extreme magnitudes and
     *        SHORTEST picks the shorter spelling. The correctly-rounded scientific core is
     *        the same one the default form already uses at the extremes.
     */
    PROVEN_FLOAT_FORMAT_MODE_SCIENTIFIC = 2,
    /**
     * @brief printf's %g (C17 7.21.6.1): `precision` SIGNIFICANT digits (0 means 1). The value
     *        is rounded to them first; with X the exponent of the rounded value, the fixed
     *        form with precision - 1 - X decimals when precision > X >= -4, else the
     *        scientific form with precision - 1; then trailing zeros, and a point left bare,
     *        are removed unless `alternate` is set. 0.1 + 0.2 at 6 digits is "0.3"; 999999.5
     *        is "1e+06". Correctly rounded, like the other modes.
     */
    PROVEN_FLOAT_FORMAT_MODE_GENERAL = 3,
} proven_float_format_mode_t;

typedef struct {
    proven_float_format_mode_t mode;
    proven_i32 precision;
    /**
     * @brief Never fall back to scientific notation, whatever the magnitude.
     *
     * FIXED mode still switches to `%e` outside roughly [1e-4, 1e18), because that is
     * what the default `{}` form wants. But `%f` means "no exponent, ever" everywhere
     * else in the world, so `{:f}` needs a way to say it - and without this flag it
     * could not: `{:f}` on 1e20 produced "1.000000e+20", which is precisely the
     * fixed form it was asked to force.
     */
    bool never_scientific;
    /**
     * @brief printf's `#` (the alternative form), FIXED, SCIENTIFIC and GENERAL: the decimal
     *        point is always written, even with no digit after it; GENERAL keeps its trailing
     *        zeros.
     */
    bool alternate;
    /** @brief `E` for the exponent's letter, and `INF`/`NAN` under `printf_specials` (%E, %G). */
    bool uppercase;
    /**
     * @brief Infinity and NaN as printf spells them: `inf`, `-inf`, `nan`, and `-nan` for a NaN
     *        whose sign bit is set (glibc's spelling). Off: `Inf`, `-Inf`, `NaN` - the library's
     *        own, which `{}` and `{:r}` keep.
     */
    bool printf_specials;
} proven_float_format_options_t;

static inline proven_float_format_options_t proven_float_format_options_fixed_default(void) {
    return (proven_float_format_options_t){ .mode = PROVEN_FLOAT_FORMAT_MODE_FIXED, .precision = 6 };
}

static inline proven_float_format_options_t proven_float_format_options_shortest(void) {
    return (proven_float_format_options_t){ .mode = PROVEN_FLOAT_FORMAT_MODE_SHORTEST, .precision = 0 };
}

/** @brief Always-scientific with 6 digits after the point, like printf's default %e. */
static inline proven_float_format_options_t proven_float_format_options_scientific(void) {
    return (proven_float_format_options_t){ .mode = PROVEN_FLOAT_FORMAT_MODE_SCIENTIFIC, .precision = 6 };
}

/** @brief printf's %g: six significant digits, its spellings of infinity and NaN. */
static inline proven_float_format_options_t proven_float_format_options_general(void) {
    return (proven_float_format_options_t){ .mode = PROVEN_FLOAT_FORMAT_MODE_GENERAL, .precision = 6, .printf_specials = true };
}

[[nodiscard]]
proven_err_t proven_float_format_f64_policy(char *buf, proven_size_t buf_cap, double value,
                                            proven_float_format_policy_t policy,
                                            proven_float_format_options_t opt,
                                            proven_size_t *written_out);

[[nodiscard]]
proven_err_t proven_float_format_f32_policy(char *buf, proven_size_t buf_cap, float value,
                                            proven_float_format_policy_t policy,
                                            proven_float_format_options_t opt,
                                            proven_size_t *written_out);

#endif /* PROVEN_FLOAT_FORMAT_H */
