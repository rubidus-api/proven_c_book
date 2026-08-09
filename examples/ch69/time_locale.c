/* LC_TIME — 같은 시각이 로케일에 따라 다르게 적힌다. 그리고 안 바뀌는 서식. */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* 재현 가능한 시각을 쓴다: 2026-08-06 (목) 15:04:05 UTC */
static struct tm fixed_time(void)
{
    struct tm t = {
        .tm_year = 2026 - 1900, .tm_mon = 8 - 1, .tm_mday = 6,
        .tm_hour = 15, .tm_min = 4, .tm_sec = 5,
        .tm_wday = 4,               /* 목요일 */
        .tm_yday = 217, .tm_isdst = 0,
    };
    return t;
}

static void row(const char *locale, const char *fmt)
{
    char buf[256];
    struct tm t = fixed_time();

    if (!setlocale(LC_TIME, locale)) {
        printf("  %-14s (이 기계에 없음)\n", locale);
        return;
    }
    size_t n = strftime(buf, sizeof buf, fmt, &t);
    if (n == 0) { printf("  %-14s (버퍼 부족)\n", locale); return; }
    printf("  %-14s %s\n", locale, buf);
}

int main(void)
{
    static const char *locales[] = {
        "C", "en_US.UTF-8", "ko_KR.UTF-8", "de_DE.UTF-8",
        "fr_FR.UTF-8", "ja_JP.UTF-8",
    };
    static const struct { const char *fmt, *what; } cases[] = {
        { "%c",       "%c  — 로케일이 정한 '날짜와 시각' 표기" },
        { "%x",       "%x  — 로케일이 정한 날짜 표기" },
        { "%X",       "%X  — 로케일이 정한 시각 표기" },
        { "%A %B",    "%A %B — 요일과 달의 전체 이름" },
        { "%a %b",    "%a %b — 요일과 달의 줄임 이름" },
        { "%p %I:%M", "%p %I:%M — 오전/오후와 12시간제" },
        { "%F %T",    "%F %T — ISO 8601. *로케일과 무관하다*" },
        { "%Y-%m-%d", "%Y-%m-%d — 숫자 서식도 로케일과 무관하다" },
    };

    for (size_t c = 0; c < sizeof cases / sizeof *cases; c++) {
        printf("\n%s\n", cases[c].what);
        for (size_t i = 0; i < sizeof locales / sizeof *locales; i++)
            row(locales[i], cases[c].fmt);
    }

    /* 시간대는 로케일이 아니라 TZ 환경 변수가 정한다 — 흔한 혼동이다. */
    puts("\n시간대는 LC_TIME 이 아니라 TZ 가 정한다:");
    setlocale(LC_TIME, "C");
    struct tm t = fixed_time();
    char buf[128];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %Z(%z)", &t);
    printf("  %s\n", buf);
    return 0;
}
