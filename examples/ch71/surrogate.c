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
    if (n == 0)      printf("cannot be represented    ");
    else if (n == 1) printf("%04X        one unit         ", u16[0]);
    else             printf("%04X %04X   back again U+%04lX", u16[0], u16[1],
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
    printf("  UTF-8 %2zu bytes  code points %zu  UTF-16 %zu units   %s\n",
           bytes, cps, u16units, label);
}

int main(void)
{
    puts("[code point -> UTF-16]");
    report("'A'", 0x41);
    report("'한'", 0xD55C);
    report("U+FFFD replacement character", 0xFFFD);
    report("a surrogate itself, D800", 0xD800);
    report("emoji U+1F600", 0x1F600);
    report("CJK extension U+2A6B2", 0x2A6B2);
    report("last of Unicode", 0x10FFFF);
    report("out of range", 0x110000);

    puts("\nthe arithmetic goes like this (U+1F600):");
    unsigned long v = 0x1F600UL - 0x10000UL;
    printf("  0x1F600 - 0x10000 = 0x%05lX (20 bits)\n", v);
    printf("  high 10 bits 0x%03lX + 0xD800 = 0x%04lX\n", v >> 10, 0xD800UL + (v >> 10));
    printf("  low  10 bits 0x%03lX + 0xDC00 = 0x%04lX\n",
           v & 0x3FF, 0xDC00UL + (v & 0x3FF));

    const char *loc = setlocale(LC_CTYPE, "C.UTF-8");
    if (!loc) loc = setlocale(LC_CTYPE, "en_US.UTF-8");
    if (!loc) { puts("\nno UTF-8 locale, so the length demonstration is skipped"); return 0; }

    puts("\n[three lengths of the same string]");
    lengths("\"Hi\"", "Hi");
    lengths("\"한글\"", "한글");
    lengths("one emoji", "\xF0\x9F\x98\x80");
    lengths("mixed", "a한\xF0\x9F\x98\x80");
    printf("  wchar_t here is %zu bytes -> UTF-32. On Windows it is 2 bytes -> UTF-16.\n",
           sizeof(wchar_t));
    return 0;
}
