#include "proven_sys_time.h"

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

unsigned long long proven_sys_time_now_ns(void) {
    // Windows: Use FileTime to match Unix Epoch
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    // 100-ns intervals since 1601 to ns since 1970
    return (ull.QuadPart - 116444736000000000ULL) * 100;
}

void proven_sys_time_now_local(proven_sys_datetime_t *out_dt) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    out_dt->year = st.wYear;
    out_dt->month = st.wMonth;
    out_dt->day = st.wDay;
    out_dt->hour = st.wHour;
    out_dt->min = st.wMinute;
    out_dt->sec = st.wSecond;
    out_dt->ms = st.wMilliseconds;
}

void proven_sys_time_sleep_ms(unsigned int ms) {
    Sleep(ms);
}
#else
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

unsigned long long proven_sys_time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ULL + (unsigned long long)ts.tv_nsec;
}

void proven_sys_time_now_local(proven_sys_datetime_t *out_dt) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_val;
    localtime_r(&tv.tv_sec, &tm_val);
    
    out_dt->year = tm_val.tm_year + 1900;
    out_dt->month = tm_val.tm_mon + 1;
    out_dt->day = tm_val.tm_mday;
    out_dt->hour = tm_val.tm_hour;
    out_dt->min = tm_val.tm_min;
    out_dt->sec = tm_val.tm_sec;
    out_dt->ms = (int)(tv.tv_usec / 1000);
}

void proven_sys_time_sleep_ms(unsigned int ms) {
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000UL);
    while (nanosleep(&req, &req) != 0) {
        /* retry if interrupted */
    }
}
#endif
