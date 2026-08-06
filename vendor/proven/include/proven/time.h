#ifndef PROVEN_TIME_H
#define PROVEN_TIME_H

#include "types.h"
#include "error.h"
#include "u8str.h"
#ifndef PROVEN_NO_U16STR
#include "u16str.h"
#endif

/**
 * @brief High-precision timestamp in nanoseconds since UNIX epoch.
 */
typedef proven_i64 proven_time_t;

/**
 * @brief Human-readable broken-down time.
 */
typedef struct {
    proven_i32 year;    /**< Year (e.g., 2026) */
    proven_u8  month;   /**< Month of the year [1, 12] */
    proven_u8  day;     /**< Day of the month [1, 31] */
    proven_u8  hour;    /**< Hours since midnight [0, 23] */
    proven_u8  min;     /**< Minutes after the hour [0, 59] */
    proven_u8  sec;     /**< Seconds after the minute [0, 60] */
    proven_u32 ms;      /**< Milliseconds after the second [0, 999] */
    proven_u8  weekday; /**< Day of the week [0, 6] (0 = Sunday) */
} proven_datetime_t;

/**
 * @brief Locale configuration for time formatting.
 */
typedef struct {
    const proven_u8str_view_t *month_names;          /**< Array of 12 full month name views */
    const proven_u8str_view_t *month_short_names;    /**< Array of 12 short month name views */
    const proven_u8str_view_t *weekday_names;        /**< Array of 7 full weekday name views */
    const proven_u8str_view_t *weekday_short_names;  /**< Array of 7 short weekday name views */
} proven_time_locale_t;

/**
 * @brief Default English locale instance.
 */
extern const proven_time_locale_t proven_time_locale_en;

/**
 * @brief Formats a datetime struct into a string using a custom format.
 * 
 * Supports dynamic field insertion with formatting specifiers matching `fmt.h`.
 * Keys available:
 * - {year}, {month}, {day}, {hour}, {min}, {sec}, {ms}, {wday_num}
 * - {Month}, {mon} (Requires locale)
 * - {Weekday}, {wday} (Requires locale)
 * 
 * Example:
 *   proven_time_u8_fmt(alloc, &str, dt, &proven_time_locale_en, "{year}-{month:0>2}-{day:0>2} {Weekday}");
 * 
 * @param alloc  Allocator to use for expanding the string.
 * @param str    String to append the formatted text to.
 * @param dt     Datetime struct containing the time data.
 * @param locale Locale to use. If NULL, defaults to `proven_time_locale_en`.
 * @param fmt    The format string layout.
 * @return proven_err_t PROVEN_OK on success.
 */
proven_err_t proven_time_u8_fmt(proven_allocator_t alloc, proven_u8str_t *str, proven_datetime_t dt, const proven_time_locale_t *locale, const char *fmt);

/**
 * @brief Formats a datetime struct into a u16 string.
 */
#ifndef PROVEN_NO_U16STR
proven_err_t proven_time_u16_fmt(proven_allocator_t alloc, proven_u16str_t *str, proven_datetime_t dt, const proven_time_locale_t *locale, const char *fmt);
#endif

/**
 * @brief Get the current high-precision timestamp in nanoseconds.
 */
[[nodiscard]]
proven_time_t proven_time_now(void);

/**
 * @brief Convert nanoseconds since epoch to broken-down UTC time.
 */
[[nodiscard]]
proven_datetime_t proven_time_breakdown(proven_time_t time_ns);

/**
 * @brief Get the current local time as broken-down structure.
 */
[[nodiscard]]
proven_datetime_t proven_time_now_datetime(void);

/**
 * @brief Sleep for a specified duration in milliseconds.
 */
void proven_time_sleep(proven_u32 ms);

#endif /* PROVEN_TIME_H */
