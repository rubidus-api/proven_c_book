/* 다섯 가지 문자 상수와 문자열 — 타입, 요소 크기, 그리고 실제 바이트. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <uchar.h>
#include <wchar.h>

/* 어떤 배열이든 요소 하나씩 16진수로 늘어놓는다.
   char 은 부호 있는 구현이 흔하므로(26장) 요소 크기만큼 잘라 낸다 —
   그러지 않으면 0xED 가 FFFFFFED 로 늘어나 보인다. */
#define DUMP(label, arr)                                                     \
    do {                                                                     \
        printf("  %-10s 요소 %zu바이트 × %zu개:", (label),                   \
               sizeof (arr)[0], sizeof (arr) / sizeof (arr)[0]);             \
        for (size_t i = 0; i < sizeof (arr) / sizeof (arr)[0]; i++)          \
            printf(" %0*llX", (int)(sizeof (arr)[0] * 2),                    \
                   (unsigned long long)(arr)[i]                              \
                   & ((1ULL << (8 * sizeof (arr)[0])) - 1));                 \
        putchar('\n');                                                       \
    } while (0)

int main(void)
{
    puts("[문자 상수] 같은 'A' 를 다섯 가지로 적으면");
    printf("  %-14s 타입 크기 %zu, 값 %d\n",  "'A'",   sizeof('A'), 'A');
    printf("  %-14s 타입 크기 %zu, 값 %ld\n", "L'A'",  sizeof(L'A'), (long)L'A');
    printf("  %-14s 타입 크기 %zu, 값 %u\n",  "u'A'",  sizeof(u'A'), (unsigned)u'A');
    printf("  %-14s 타입 크기 %zu, 값 %u\n",  "U'A'",  sizeof(U'A'), (unsigned)U'A');
    printf("  %-14s 타입 크기 %zu, 값 %u\n",  "u8'A'", sizeof(u8'A'), (unsigned)u8'A');
    puts("  ('A' 는 int 다 — C 에서 문자 상수의 타입은 char 가 아니다)");

    puts("\n[문자열] \"한\" 하나를 다섯 가지로 적으면");
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

    puts("\n[BMP 밖의 글자] U+1F600 (웃는 얼굴)");
    static const char     e_plain[] =   "\U0001F600";
    static const char16_t e_u16[]   =  u"\U0001F600";
    static const char32_t e_u32[]   =  U"\U0001F600";
    static const wchar_t  e_wide[]  =  L"\U0001F600";
    DUMP("char",     e_plain);
    DUMP("char16_t", e_u16);   /* ← 두 요소가 된다: 서러게이트 쌍 */
    DUMP("char32_t", e_u32);
    DUMP("wchar_t",  e_wide);
    puts("  char16_t 만 요소가 둘이다 — 16비트로는 담기지 않아 쌍으로 쪼갠다.");

    puts("\n[구현이 밝히는 것]");
#ifdef __STDC_ISO_10646__
    printf("  __STDC_ISO_10646__ = %ldL — wchar_t 값이 유니코드 코드포인트다\n",
           (long)__STDC_ISO_10646__);
#else
    puts("  __STDC_ISO_10646__ 없음 — wchar_t 인코딩은 구현이 정한다(윈도가 그렇다)");
#endif
#ifdef __STDC_UTF_16__
    puts("  __STDC_UTF_16__  = 1 — char16_t 는 UTF-16 이다");
#endif
#ifdef __STDC_UTF_32__
    puts("  __STDC_UTF_32__  = 1 — char32_t 는 UTF-32 이다");
#endif
    printf("  sizeof(wchar_t) = %zu, WCHAR_MIN = %ld, WCHAR_MAX = %lld\n",
           sizeof(wchar_t), (long)WCHAR_MIN, (long long)WCHAR_MAX);
    return 0;
}
