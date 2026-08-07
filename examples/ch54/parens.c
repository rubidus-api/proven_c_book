/* 매크로 인자를 괄호로 감싸야 하는 이유 — 의도와 전개가 갈라지는 자리들.
   xstr() 로 "그래서 무엇으로 펼쳐졌는가"를 나란히 찍어 확인한다. */
#include <stdio.h>

#define str(...)   # __VA_ARGS__
#define xstr(...)  str(__VA_ARGS__)

#define SQ_BAD(x)   x * x           /* 괄호 없음 */
#define SQ_HALF(x)  (x) * (x)       /* 인자만 감쌈 */
#define SQ(x)       ((x) * (x))     /* 인자와 전체를 감쌈 — 정석 */

#define NEG_BAD(x)  -x
#define NEG(x)      (-(x))

/* 인자를 두 번 쓰는 매크로 — 부수효과가 있는 인자를 만나면 */
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

int main(void)
{
    /* ── ① 인자를 안 감싸면 — 우선순위가 끼어든다 ─────────────── */
    printf("SQ_BAD(1+2)  -> %-16s = %d   (의도는 9)\n",
           xstr(SQ_BAD(1+2)), SQ_BAD(1+2));
    printf("SQ_HALF(1+2) -> %-16s = %d\n",
           xstr(SQ_HALF(1+2)), SQ_HALF(1+2));

    /* ── ② 전체를 안 감싸면 — 바깥 연산자가 끼어든다 ──────────── */
    printf("100/SQ_HALF(2) -> %-14s = %d   (의도는 25)\n",
           xstr(100/SQ_HALF(2)), 100/SQ_HALF(2));
    printf("100/SQ(2)      -> %-14s = %d\n",
           xstr(100/SQ(2)), 100/SQ(2));

    /* ── ③ 부호가 붙은 인자 ──────────────────────────────────── */
    printf("NEG_BAD(-3)  -> %-16s  ← 컴파일 오류다(`--3`)\n", xstr(NEG_BAD(-3)));
    printf("NEG(-3)      -> %-16s = %d\n", xstr(NEG(-3)), NEG(-3));

    /* ── ④ 괄호로도 못 막는 것 — 인자를 두 번 평가한다 ────────── */
    int i = 5, j = 3;
    int m = MAX(i++, j);
    printf("MAX(i++, j)  -> i 가 %d 가 되었다 (한 번만 증가할 줄 알았는데)\n", i);
    printf("             결과 m = %d\n", m);
    printf("             전개: %s\n", xstr(MAX(i++, j)));
    return 0;
}
