/* UTF-16 의 한계 — 서러게이트 쌍, 그리고 '길이'가 세 가지인 이유. */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

/* 코드포인트 하나를 UTF-16 단위로 쪼갠다.
   BMP(U+0000~U+FFFF, 단 D800~DFFF 제외)는 그대로 한 단위,
   그 위(U+10000~U+10FFFF)는 두 단위 — 서러게이트 쌍이다. */
static int to_utf16(unsigned long cp, char16_t out[2])
{
    if (cp > 0x10FFFFUL) return 0;                 /* 유니코드 범위 밖 */
    if (cp >= 0xD800UL && cp <= 0xDFFFUL) return 0; /* 서러게이트 자체는 문자가 아니다 */
    if (cp < 0x10000UL) { out[0] = (char16_t)cp; return 1; }

    unsigned long v = cp - 0x10000UL;              /* 20비트로 줄인다 */
    out[0] = (char16_t)(0xD800UL + (v >> 10));     /* 상위 10비트 → 고위 서러게이트 */
    out[1] = (char16_t)(0xDC00UL + (v & 0x3FFUL)); /* 하위 10비트 → 저위 서러게이트 */
    return 2;
}

/* 반대 방향 — 쌍을 다시 코드포인트로 */
static unsigned long from_pair(char16_t hi, char16_t lo)
{
    return 0x10000UL + (((unsigned long)hi - 0xD800UL) << 10)
                     +  ((unsigned long)lo - 0xDC00UL);
}

static void report(const char *name, unsigned long cp)
{
    char16_t u16[2] = { 0, 0 };
    int n = to_utf16(cp, u16);

    char cpname[16];
    snprintf(cpname, sizeof cpname, "U+%04lX", cp);
    printf("  %-9s → ", cpname);
    if (n == 0)      printf("담을 수 없다               ");
    else if (n == 1) printf("%04X        한 단위          ", u16[0]);
    else             printf("%04X %04X   되돌리면 U+%04lX", u16[0], u16[1],
                             from_pair(u16[0], u16[1]));
    printf("  %s\n", name);
}

/* 문자열 하나를 세 가지 길이로 잰다 */
static void lengths(const char *label, const char *utf8)
{
    size_t bytes = strlen(utf8);

    /* 코드포인트 수와 UTF-16 단위 수를 직접 센다 */
    size_t cps = 0, u16units = 0;
    mbstate_t st;
    memset(&st, 0, sizeof st);
    for (size_t pos = 0; pos < bytes; ) {
        wchar_t wc = 0;
        size_t r = mbrtowc(&wc, utf8 + pos, bytes - pos, &st);
        if (r == (size_t)-1 || r == (size_t)-2) break;
        if (r == 0) r = 1;
        pos += r;
        cps++;
        char16_t tmp[2];
        u16units += (size_t)to_utf16((unsigned long)wc, tmp);
    }
    printf("  UTF-8 %2zu바이트  코드포인트 %zu개  UTF-16 %zu단위   %s\n",
           bytes, cps, u16units, label);
}

int main(void)
{
    puts("[코드포인트 → UTF-16]");
    report("'A'", 0x41);
    report("'한'", 0xD55C);
    report("U+FFFD 대체 문자", 0xFFFD);
    report("서러게이트 자체 D800", 0xD800);
    report("이모지 U+1F600", 0x1F600);
    report("한자 확장 U+2A6B2", 0x2A6B2);
    report("유니코드 마지막", 0x10FFFF);
    report("범위 밖", 0x110000);

    puts("\n계산은 이렇게 나온다 (U+1F600):");
    unsigned long v = 0x1F600UL - 0x10000UL;
    printf("  0x1F600 - 0x10000 = 0x%05lX (20비트)\n", v);
    printf("  상위 10비트 0x%03lX + 0xD800 = 0x%04lX\n", v >> 10, 0xD800UL + (v >> 10));
    printf("  하위 10비트 0x%03lX + 0xDC00 = 0x%04lX\n",
           v & 0x3FF, 0xDC00UL + (v & 0x3FF));

    const char *loc = setlocale(LC_CTYPE, "C.UTF-8");
    if (!loc) loc = setlocale(LC_CTYPE, "en_US.UTF-8");
    if (!loc) { puts("\nUTF-8 로케일이 없어 길이 시연은 건너뛴다"); return 0; }

    puts("\n[같은 문자열의 세 가지 길이]");
    lengths("\"Hi\"", "Hi");
    lengths("\"한글\"", "한글");
    lengths("이모지 하나", "\xF0\x9F\x98\x80");
    lengths("섞인 것", "a한\xF0\x9F\x98\x80");
    printf("  이 구현의 wchar_t: %zu바이트 → UTF-32. 윈도는 2바이트 → UTF-16.\n",
           sizeof(wchar_t));
    return 0;
}
