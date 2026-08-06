/* 두 겹 우회(double expansion) — C 표준이 직접 든 예제를 그대로 돌려 본다.
   한 겹과 두 겹의 결과가 어떻게 갈리는지가 전부 여기 있다. */
#include <stdio.h>

/* ── 표준 예제의 매크로들 (C17 §6.10.3.5) ─────────────────────── */
#define str(s)       # s              /* 문자열화(stringize) */
#define xstr(s)      str(s)           /* 한 겹 더 두른 판 */
#define INCFILE(n)   vers ## n        /* 토큰 붙이기(token pasting) */
#define glue(a, b)   a ## b
#define xglue(a, b)  glue(a, b)       /* 한 겹 더 두른 판 */
#define HIGHLOW      "hello"
#define LOW          LOW ", world"    /* 자기 자신을 부른다 — 파란 칠 규칙의 무대 */

/* 조립된 이름이 헤더 이름이 된다: xstr(INCFILE(2).h) -> "vers2.h" */
#include xstr(INCFILE(2).h)

#define WIDTH 80

int main(void)
{
    /* ── ① 문자열화: 한 겹은 안 풀리고, 두 겹은 풀린다 ────────── */
    printf("str(WIDTH)   = %s\n", str(WIDTH));    /* 인자가 # 의 피연산자 */
    printf("xstr(WIDTH)  = %s\n", xstr(WIDTH));   /* 먼저 펼친 뒤 문자열로 */

    /* ── ② 붙이기: 표준 예제의 그 두 줄 ──────────────────────── */
    printf("glue(HIGH, LOW)  = %s\n", glue(HIGH, LOW));
    printf("xglue(HIGH, LOW) = %s\n", xglue(HIGH, LOW));

    /* ── ③ 조립한 이름으로 포함한 헤더가 실제로 왔는지 ───────── */
    printf("VERS_TAG = %s\n", VERS_TAG);

    /* ── ④ 표준 예제의 문자열화 두 가지 ──────────────────────── */
    printf("%s\n", str(strncmp("abc\0d", "abc", '\4') == 0));
    return 0;
}
