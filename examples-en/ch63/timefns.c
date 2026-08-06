#include <stdio.h>
#include <time.h>
#include <string.h>

int main(void)
{
    /* A fixed moment is used so the output is the same every time (a reproducible example) */
    struct tm t = {0};
    t.tm_year = 2026 - 1900;   /* * 1900 subtracted */
    t.tm_mon  = 8 - 1;         /* * counted from 0 */
    t.tm_mday = 5;
    t.tm_hour = 13; t.tm_min = 45; t.tm_sec = 30;
    t.tm_isdst = -1;           /* marks that we do not know about daylight saving */

    printf("the trap in the struct fields: tm_year=%d (the year 2026), tm_mon=%d (August)\n",
           t.tm_year, t.tm_mon);

    /* mktime normalises the values and fills in the day of the week */
    struct tm norm = t;
    time_t stamp = mktime(&norm);
    static const char *dow[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    printf("the weekday mktime filled in: %s (tm_wday=%d)\n",
           norm.tm_wday >= 0 && norm.tm_wday < 7 ? dow[norm.tm_wday] : "?", norm.tm_wday);

    /* strftime takes the buffer size and returns 0 if it does not fit */
    char buf[64];
    size_t n = strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &norm);
    printf("strftime: [%s] (%zu characters)\n", buf, n);

    char tiny[8];
    size_t m = strftime(tiny, sizeof tiny, "%Y-%m-%d %H:%M:%S", &norm);
    printf("a small buffer: it returns %zu (0 means failure — the contents are unspecified)\n", m);

    /* date arithmetic is done with mktime, not by subtracting seconds */
    struct tm plus = norm;
    plus.tm_mday += 30;              /* 30 days later — mktime tidies it up even across a month boundary */
    time_t later = mktime(&plus);
    strftime(buf, sizeof buf, "%Y-%m-%d", &plus);
    printf("30 days later: %s\n", buf);
    printf("difftime: %.0f seconds\n", difftime(later, stamp));

    /* for measuring elapsed time use clock() or the platform's monotonic clock */
    clock_t c0 = clock();
    volatile long acc = 0;
    for (long i = 0; i < 1000000; i++) acc += i;
    clock_t c1 = clock();
    printf("time measured with clock(): %s\n",
           (double)(c1 - c0) / CLOCKS_PER_SEC >= 0.0 ? "measured (the value differs by machine)" : "?");
    return 0;
}
