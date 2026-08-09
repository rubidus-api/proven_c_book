/* 상수를 적는 방법들 — 표기가 값과 타입을 어떻게 정하는가. */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <uchar.h>

int main(void)
{
    puts("[integer constants - four bases and digit separators (C23)]");
    printf("  1234        = %d\n", 1234);          /* 10진 */
    printf("  0755        = %d   <- a leading 0 means octal\n", 0755);
    printf("  0xFF        = %d\n", 0xFF);
    printf("  0b1010      = %d   <- binary, new in C23\n", 0b1010);
    printf("  1'000'000   = %d   <- digit separators, new in C23\n", 1'000'000);
    printf("  0b11'10'11'01 = %d\n", 0b11'10'11'01);

    puts("\n[접미어가 타입을 정한다]");
    printf("  sizeof 1    = %zu, sizeof 1L  = %zu, sizeof 1LL = %zu\n",
           sizeof 1, sizeof 1L, sizeof 1LL);
    printf("  sizeof 1U   = %zu, sizeof 1wb = %zu  ← wb 는 C23 의 _BitInt\n",
           sizeof 1U, sizeof 1wb);

    puts("\n[문자 상수 — 접두어가 타입을 정한다]");
    printf("  'a'   크기 %zu, 값 %d      ← C 에서 문자 상수는 int 다\n",
           sizeof 'a', 'a');
    printf("  u8'a' 크기 %zu (char8_t)\n",  sizeof u8'a');
    printf("  u'a'  크기 %zu (char16_t)\n", sizeof u'a');
    printf("  U'a'  크기 %zu (char32_t)\n", sizeof U'a');
    printf("  L'a'  크기 %zu (wchar_t)\n",  sizeof L'a');
    /* 'ab' 같은 다중 문자 상수는 값이 구현 정의라 -Wmultichar 가 경고한다.
       여기서는 경고를 켠 채 두고, 값은 본문의 실측 표로만 보인다. */

    puts("\n[이스케이프 — 8진과 16진]");
    printf("  '\\101' = %d, '\\x41' = %d   ← 둘 다 'A'\n", '\101', '\x41');
    printf("  \"\\x41\" \"1\" = \"%s\"        ← 16진은 가장 긴 열을 먹는다.\n",
           "\x41" "1");
    puts("    그래서 \"\\x411\" 이 아니라 문자열을 쪼개 이어 붙인다.");

    puts("\n[부동소수점 상수]");
    printf("  3.14  1e3=%g  1.=%g  .5=%g\n", 1e3, 1., .5);
    printf("  0x1p-3 = %g            ← 16진 부동소수점. 지수부 p 는 필수다\n", 0x1p-3);
    printf("  sizeof 1.0 = %zu, 1.0f = %zu, 1.0L = %zu\n",
           sizeof 1.0, sizeof 1.0f, sizeof 1.0L);
    printf("  0.1 == 0.1f ? %s   ← 접미어가 다르면 값도 다르다\n",
           (double)0.1f == 0.1 ? "예" : "아니오");

    puts("\n[문자열 리터럴]");
    printf("  sizeof \"abc\" = %zu        ← NUL 이 한 칸 더 붙는다\n", sizeof "abc");
    printf("  \"hello, \" \"world\" = \"%s\"  ← 인접한 것은 하나로 이어진다\n",
           "hello, " "world");
    printf("  \"a\\0b\": strlen = %zu, sizeof = %zu  ← 안에 NUL 을 넣어도 배열은 남는다\n",
           strlen("a\0b"), sizeof "a\0b");
    printf("  sizeof u8\"가\" = %zu, sizeof u\"가\" = %zu, sizeof U\"가\" = %zu\n",
           sizeof u8"\uAC00", sizeof u"\uAC00", sizeof U"\uAC00");
    return 0;
}
