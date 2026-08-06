#ifndef PROVEN_FLOAT_PARSE_H
#define PROVEN_FLOAT_PARSE_H

#include "proven/types.h"
#include "proven/error.h"
#include "proven/u8str.h"

typedef struct {
    proven_err_t err;
    double val;
    proven_size_t consumed;
} proven_parse_f64_result_t;

typedef proven_parse_f64_result_t proven_parse_double_result_t;

/*
 * Locale-free ASCII decimal parser for binary64.
 *
 * The parser accepts:
 * - optional sign
 * - decimal digits with optional decimal point
 * - optional exponent part with e/E and optional sign
 * - inf / infinity / nan / nan(...)
 *
 * It does not skip leading whitespace. Callers that need strtod-style
 * whitespace handling should use proven_strtod().
 *
 * On success, consumed reports the number of bytes used for the conversion.
 * On failure, err reports the parse error and consumed is zero.
 */
[[nodiscard]]
proven_parse_double_result_t proven_parse_double_ascii(proven_u8str_view_t input);

/*
 * Compatibility alias for callers that adopted the earlier f64-specific name
 * before the RFC-0001 public parser seam was finalized.
 */
[[nodiscard]]
proven_parse_f64_result_t proven_parse_f64_ascii(proven_u8str_view_t input);

/*
 * strtod-like wrapper over the shared ASCII parser.
 *
 * The wrapper:
 * - skips leading ASCII whitespace
 * - updates endptr to the first byte after the parsed token
 * - returns signed infinity on overflow and preserves signed zero on underflow
 *
 * Hosted builds also set errno to ERANGE on overflow and underflow. Freestanding
 * builds keep the same value/result behavior without requiring errno support.
 */
[[nodiscard]]
double proven_strtod(const char *nptr, char **endptr);

#endif /* PROVEN_FLOAT_PARSE_H */
