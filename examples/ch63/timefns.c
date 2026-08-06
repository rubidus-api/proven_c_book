#include <stdio.h>
#include <time.h>
#include <string.h>

int main(void)
{
    /* 고정된 시각을 써서 출력이 매번 같게 한다 (재현 가능한 예제) */
    struct tm t = {0};
    t.tm_year = 2026 - 1900;   /* ★ 1900 을 뺀 값 */
    t.tm_mon  = 8 - 1;         /* ★ 0 부터 시작 */
    t.tm_mday = 5;
    t.tm_hour = 13; t.tm_min = 45; t.tm_sec = 30;
    t.tm_isdst = -1;           /* 서머타임 여부를 모른다고 표시 */

    printf("구조체 필드의 함정: tm_year=%d (2026년), tm_mon=%d (8월)\n",
           t.tm_year, t.tm_mon);

    /* mktime 은 값을 정규화하고 요일을 채워 준다 */
    struct tm norm = t;
    time_t stamp = mktime(&norm);
    static const char *dow[] = {"일","월","화","수","목","금","토"};
    printf("mktime 이 채운 요일: %s요일 (tm_wday=%d)\n",
           norm.tm_wday >= 0 && norm.tm_wday < 7 ? dow[norm.tm_wday] : "?", norm.tm_wday);

    /* strftime 은 버퍼 크기를 받고, 넘치면 0 을 돌려준다 */
    char buf[64];
    size_t n = strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &norm);
    printf("strftime: [%s] (%zu 글자)\n", buf, n);

    char tiny[8];
    size_t m = strftime(tiny, sizeof tiny, "%Y-%m-%d %H:%M:%S", &norm);
    printf("작은 버퍼: 반환 %zu (0 이면 실패 — 내용은 미정)\n", m);

    /* 날짜 산술은 초 단위 뺄셈이 아니라 mktime 으로 한다 */
    struct tm plus = norm;
    plus.tm_mday += 30;              /* 30일 뒤 — 달을 넘겨도 mktime 이 정리한다 */
    time_t later = mktime(&plus);
    strftime(buf, sizeof buf, "%Y-%m-%d", &plus);
    printf("30일 뒤: %s\n", buf);
    printf("difftime: %.0f 초\n", difftime(later, stamp));

    /* 경과 시간 측정에는 clock() 이나 플랫폼의 단조 시계를 쓴다 */
    clock_t c0 = clock();
    volatile long acc = 0;
    for (long i = 0; i < 1000000; i++) acc += i;
    clock_t c1 = clock();
    printf("clock() 로 잰 시간: %s\n",
           (double)(c1 - c0) / CLOCKS_PER_SEC >= 0.0 ? "측정됨(값은 기계마다 다름)" : "?");
    return 0;
}
