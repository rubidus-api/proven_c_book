/* 다섯 가지 문자 상수와 문자열 — 타입, 요소 크기, 그리고 실제 바이트. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <uchar.h>
#include <wchar.h>

/* 어떤 배열이든 요소 하나씩 16진수로 늘어놓는다.
   char 은 부호 있는 구현이 흔하므로(27장) 요소 크기만큼 잘라 낸다 —
   그러지 않으면 0xED 가 FFFFFFED 로 늘어나 보인다. */
#define DUMP(label, arr)                                                     \
    do {                                                                     \
        printf("  %-10s %zu elements of %zu bytes: ", (label),                   \
               sizeof (arr)[0], sizeof (arr) / sizeof (arr)[0]);             \
        for (size_t i = 0; i < sizeof (arr) / sizeof (arr)[0]; i++)          \
            printf(" %0*llX", (int)(sizeof (arr)[0] * 2),                    \
                   (unsigned long long)(arr)[i]                              \
                   & ((1ULL << (8 * sizeof (arr)[0])) - 1));                 \
        putchar('\n');                                                       \
    } while (0)

int main(void)
{
    puts("[character constants] the same 'A' written five ways");
    printf("  %-14s type size %zu, value %d\n",  "'A'",   sizeof('A'), 'A');
    printf("  %-14s type size %zu, value %ld\n", "L'A'",  sizeof(L'A'), (long)L'A');
    printf("  %-14s type size %zu, value %u\n",  "u'A'",  sizeof(u'A'), (unsigned)u'A');
    printf("  %-14s type size %zu, value %u\n",  "U'A'",  sizeof(U'A'), (unsigned)U'A');
    printf("  %-14s type size %zu, value %u\n",  "u8'A'", sizeof(u8'A'), (unsigned)u8'A');
    puts("  ('A' is an int - in C a character constant is not a char)");

    puts("\n[strings] the single character \"한\" written five ways");
    static const char     s_plain[] =   "한";
    static const char8_t  s_u8[]    = u8"한";
    static const char16_t s_u16[]   =  u"한";
    static const char32_t s_u32[]   =  U"한";
    static const wchar_t  s_wide[]  =  L"한";
    DUMP("char",     s_plain);
    DUMP("char8_t",  s_u8);
    DUMP("char16_t", s_u16);
    DUMP("char32_t", s_u32);
    DUMP("wchar_t",  s_wide);

    puts("\n[a character outside the BMP] U+1F600 (a smiling face)");
    static const char     e_plain[] =   "\U0001F600";
    static const char16_t e_u16[]   =  u"\U0001F600";
    static const char32_t e_u32[]   =  U"\U0001F600";
    static const wchar_t  e_wide[]  =  L"\U0001F600";
    DUMP("char",     e_plain);
    DUMP("char16_t", e_u16);   /* ← 두 요소가 된다: 서러게이트 쌍 */
    DUMP("char32_t", e_u32);
    DUMP("wchar_t",  e_wide);
    puts("  only char16_t needs two elements - it does not fit in 16 bits, so it splits into a pair.");

    puts("\n[what the implementation declares]");
#ifdef __STDC_ISO_10646__
    printf("  __STDC_ISO_10646__ = %ldL - wchar_t values are Unicode code points\n",
           (long)__STDC_ISO_10646__);
#else
    puts("  __STDC_ISO_10646__ absent - the wchar_t encoding is up to the implementation (as on Windows)");
#endif
#ifdef __STDC_UTF_16__
    puts("  __STDC_UTF_16__  = 1 - char16_t is UTF-16");
#endif
#ifdef __STDC_UTF_32__
    puts("  __STDC_UTF_32__  = 1 - char32_t is UTF-32");
#endif
    printf("  sizeof(wchar_t) = %zu, WCHAR_MIN = %ld, WCHAR_MAX = %lld\n",
           sizeof(wchar_t), (long)WCHAR_MIN, (long long)WCHAR_MAX);
    return 0;
}
