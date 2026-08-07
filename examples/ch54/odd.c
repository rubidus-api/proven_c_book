/* 표준이 직접 든 기묘한 예제들 — 전개 결과를 글자로 찍어 확인한다.
   xstr() 로 감싸면 "그래서 무엇이 되었는가"를 문자열로 볼 수 있다(두 겹 우회). */
#include <stdio.h>

/* 쉼표가 든 것도 통째로 문자열로 만들려면 가변 인자로 받는다 */
#define str(...)   # __VA_ARGS__
#define xstr(...)  str(__VA_ARGS__)

/* ── ① 자리표시자(placemarker) — 빈 인자를 ## 로 붙이면 ────────── */
#define t(x, y, z)  x ## y ## z

int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),
            t(10,,), t(,11,), t(,,12), t(,,) };

/* ── ② 표준이 "미지정"이라고 못 박은 전개 ─────────────────────── */
#define f(a)  a*g
#define g(a)  f(a)
int g = 1;                     /* g 는 함수형 매크로라 홀로 쓰면 펼쳐지지 않는다 */

/* ── ③ __VA_OPT__ — 인자가 "비었는지"를 언제 판정하는가 ───────── */
#define LOG(...)          log(0 __VA_OPT__(,) __VA_ARGS__)
#define SDEF(name, ...)   S name __VA_OPT__(= { __VA_ARGS__ })
#define EMP

int main(void)
{
    printf("① 자리표시자\n");
    printf("   j[] = {");
    for (size_t i = 0; i < sizeof j / sizeof *j; i++)
        printf(" %d", j[i]);
    printf(" }   (원소 %zu개)\n", sizeof j / sizeof *j);
    printf("   t(,,)  -> \"%s\"  (아무 토큰도 남지 않는다)\n", xstr(t(,,)));
    printf("   t(6,,7)-> \"%s\"\n", xstr(t(6,,7)));

    printf("\n② 미지정 전개\n");
    printf("   f(2)(9) -> \"%s\"\n", xstr(f(2)(9)));
    printf("   (표준은 \"2*9*g\" 또는 \"2*9*f(9)\" 중 어느 쪽인지 정하지 않는다)\n");

    printf("\n③ __VA_OPT__\n");
    printf("   LOG(1,2)  -> \"%s\"\n", xstr(LOG(1,2)));
    printf("   LOG()     -> \"%s\"\n", xstr(LOG()));
    printf("   LOG(EMP)  -> \"%s\"   ← 인자를 넘겼는데도 쉼표가 없다\n", xstr(LOG(EMP)));
    printf("   SDEF(foo)      -> \"%s\"\n", xstr(SDEF(foo)));
    printf("   SDEF(bar,1,2)  -> \"%s\"\n", xstr(SDEF(bar, 1, 2)));
    return 0;
}
