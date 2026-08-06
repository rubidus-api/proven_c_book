/* LC_TIME — one instant written differently. And the formats that never move. */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* A fixed instant so the run is reproducible: 2026-08-06 (Thu) 15:04:05 */
static struct tm fixed_time(void)
{
    struct tm t = {
        .tm_year = 2026 - 1900, .tm_mon = 8 - 1, .tm_mday = 6,
        .tm_hour = 15, .tm_min = 4, .tm_sec = 5,
        .tm_wday = 4,               /* Thursday */
        .tm_yday = 217, .tm_isdst = 0,
    };
    return t;
}

static void row(const char *locale, const char *fmt)
{
    char buf[256];
    struct tm t = fixed_time();

    if (!setlocale(LC_TIME, locale)) {
        printf("  %-14s (not on this machine)\n", locale);
        return;
    }
    size_t n = strftime(buf, sizeof buf, fmt, &t);
    if (n == 0) { printf("  %-14s (buffer too small)\n", locale); return; }
    printf("  %-14s %s\n", locale, buf);
}

int main(void)
{
    static const char *locales[] = {
        "C", "en_US.UTF-8", "ko_KR.UTF-8", "de_DE.UTF-8",
        "fr_FR.UTF-8", "ja_JP.UTF-8",
    };
    static const struct { const char *fmt, *what; } cases[] = {
        { "%c",       "%c  — the locale's own date-and-time form" },
        { "%x",       "%x  — the locale's own date form" },
        { "%X",       "%X  — the locale's own time form" },
        { "%A %B",    "%A %B — full names of the day and the month" },
        { "%a %b",    "%a %b — abbreviated names" },
        { "%p %I:%M", "%p %I:%M — am/pm and the 12-hour clock" },
        { "%F %T",    "%F %T — ISO 8601. *independent of the locale*" },
        { "%Y-%m-%d", "%Y-%m-%d — numeric forms are locale-independent too" },
    };

    for (size_t c = 0; c < sizeof cases / sizeof *cases; c++) {
        printf("\n%s\n", cases[c].what);
        for (size_t i = 0; i < sizeof locales / sizeof *locales; i++)
            row(locales[i], cases[c].fmt);
    }

    /* The time zone comes from TZ, not from the locale — a common mix-up. */
    puts("\nThe zone is set by TZ, not by LC_TIME:");
    setlocale(LC_TIME, "C");
    struct tm t = fixed_time();
    char buf[128];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %Z(%z)", &t);
    printf("  %s\n", buf);
    return 0;
}
