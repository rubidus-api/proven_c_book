#ifndef PROVEN_PLATFORM_SYS_TIME_H
#define PROVEN_PLATFORM_SYS_TIME_H

#include <stddef.h>

/**
 * @file proven_sys_time.h
 * @brief Platform Abstraction Layer for Time syscalls.
 */

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int ms;
} proven_sys_datetime_t;

#ifdef PROVEN_FREESTANDING

static inline unsigned long long proven_sys_time_now_ns(void) {
    return 0;
}

static inline void proven_sys_time_now_local(proven_sys_datetime_t *out_dt) {
    if (out_dt) {
        out_dt->year = 1970;
        out_dt->month = 1;
        out_dt->day = 1;
        out_dt->hour = 0;
        out_dt->min = 0;
        out_dt->sec = 0;
        out_dt->ms = 0;
    }
}

static inline void proven_sys_time_sleep_ms(unsigned int ms) {
    (void)ms;
}

#else

/**
 * @brief Nanoseconds since the Unix epoch, from the system's WALL clock.
 *
 * @warning Not monotonic. This header used to call it "high-resolution monotonic time",
 *          which it has never been: the POSIX path reads CLOCK_REALTIME and the Windows
 *          path GetSystemTimeAsFileTime, and both step - forwards or backwards - when NTP
 *          or an administrator adjusts the clock. Code that measured an interval by
 *          subtracting two of these could get a negative duration and believed the header.
 *          The public proven_time API documents exactly what this delivers (ns since the
 *          epoch); only this line was wrong.
 */
unsigned long long proven_sys_time_now_ns(void);

/**
 * @brief Get the current local wall-clock time broken down.
 */
void proven_sys_time_now_local(proven_sys_datetime_t *out_dt);

/**
 * @brief Sleep the current thread.
 */
void proven_sys_time_sleep_ms(unsigned int ms);

#endif /* PROVEN_FREESTANDING */

#endif /* PROVEN_PLATFORM_SYS_TIME_H */
