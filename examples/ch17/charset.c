/* 리터럴이 실제로 어떤 바이트·값이 되는가 — 층을 갈라서 본다.
   같은 소스라도 컴파일 옵션에 따라 첫 줄이 달라진다(본문의 실측 표). */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <uchar.h>

static void dump(const char *tag, const char *p, size_t n)
{
    printf("  %-28s", tag);
    for (size_t i = 0; i < n; i++) printf(" %02X", (unsigned char)p[i]);
    puts("");
}

int main(void)
{
    const char    *plain = "가";      /* 리터럴 인코딩 — 구현이 정한다 */
    const char8_t *utf8  = u8"가";    /* 언제나 UTF-8 — 표준이 못박는다 */

    puts("[문자열 리터럴의 바이트]");
    dump("\"가\"   (리터럴 인코딩)", plain, strlen(plain));
    dump("u8\"가\" (항상 UTF-8)", (const char *)utf8, strlen((const char *)utf8));

    puts("\n[넓은 문자 리터럴의 코드 단위]");
    printf("  u\"가\"[0] = U+%04X   (char16_t, UTF-16)\n",  (unsigned)u"가"[0]);
    printf("  U\"가\"[0] = U+%04X   (char32_t, UTF-32)\n",  (unsigned)U"가"[0]);
    printf("  L\"가\"[0] = U+%04X   (wchar_t, %zu바이트)\n",
           (unsigned)L"가"[0], sizeof(wchar_t));

    puts("\n[구현이 무엇을 보장한다고 말하는가]");
#ifdef __STDC_ISO_10646__
    printf("  __STDC_ISO_10646__ = %ldL  → wchar_t 가 유니코드다\n",
           (long)__STDC_ISO_10646__);
#else
    puts("  __STDC_ISO_10646__ 정의 안 됨 → wchar_t 인코딩은 구현 정의");
#endif
#ifdef __STDC_UTF_16__
    printf("  __STDC_UTF_16__    = %d      → char16_t 가 UTF-16\n", __STDC_UTF_16__);
#endif
#ifdef __STDC_UTF_32__
    printf("  __STDC_UTF_32__    = %d      → char32_t 가 UTF-32\n", __STDC_UTF_32__);
#endif

    puts("\n[기본 문자 집합은 흔들리지 않는다]");
    printf("  'A' = %d, '0' = %d, sizeof \"A\" = %zu\n", 'A', '0', sizeof "A");
    return 0;
}
