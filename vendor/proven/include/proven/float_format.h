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
     * @brief Always scientific: "d.ddde±XX", `precision` digits after the point, whatever
     *        the magnitude. This is printf's %e - the one float form FIXED and SHORTEST do
     *        not cover, because FIXED only reaches scientific for extreme magnitudes and
     *        SHORTEST picks the shorter spelling. The correctly-rounded scientific core is
     *        the same one the default form already uses at the extremes.
     */
    PROVEN_FLOAT_FORMAT_MODE_SCIENTIFIC = 2,
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
