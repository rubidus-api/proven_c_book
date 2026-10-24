/* 대입이 하는 두 가지 — 값을 내놓는 일과 객체를 바꾸는 일. */
#include <stdio.h>

static int calls;

static int where(void)      /* 좌변의 자리를 정하는 계산도 '평가'된다 */
{
    calls++;
    printf("  where() called - it picks the slot on the left\n");
    return 1;
}

static int what(void)
{
    calls++;
    printf("  what() called - it makes the value on the right\n");
    return 42;
}

int main(void)
{
    int a[3] = {0, 0, 0};

    /* ── ① 대입은 수식이다 — 값을 낳는다 ────────────────────── */
    int x, y;
    x = (y = 7) + 1;                  /* y = 7 이라는 수식의 값은 7 */
    printf("x = (y = 7) + 1  ->  x=%d y=%d\n", x, y);

    int p, q, r;
    p = q = r = 5;                    /* 오른쪽 결합 — r 부터 채워진다 */
    printf("p = q = r = 5    ->  p=%d q=%d r=%d\n", p, q, r);

    /* ── ② 좌변도 평가된다 ──────────────────────────────────── */
    puts("\nrunning a[where()] = what(); :");
    calls = 0;
    a[where()] = what();
    printf("  both functions ran (%d calls), a[1]=%d\n", calls, a[1]);
    puts("  which one runs first is unspecified by the standard.");

    /* ── ③ 복합 대입은 왼쪽을 한 번만 평가한다 ──────────────── */
    puts("\nrunning a[where()] += 1; :");
    calls = 0;
    a[where()] += 1;
    printf("  where() call count: %d (spelled out as a[where()] = a[where()] + 1 it would be 2)\n",
           calls);
    printf("  a[1] = %d\n", a[1]);

    /* ── ④ 대입은 왼쪽 타입으로 '변환된 값'을 내놓는다 ──────── */
    char c;
    int wide = 321;
    int back = (c = (char)wide);      /* 좁혔다가 다시 넓히면 원래 값이 아니다 */
    printf("\nchar c = (char)321 -> c=%d, value of (c = ...) = %d\n", c, back);

    double d;
    int truncated = (int)(d = 3.9);   /* 실수 → 정수는 0 쪽으로 버린다 */
    printf("d = 3.9 -> d=%.1f, (int)d = %d\n", d, truncated);

    /* ── ⑤ 부수효과는 문장을 나눈다 ─────────────────────────── */
    int i = 0;
    a[i] = 10;
    i++;                              /* 한 수식에 섞지 않는다 */
    a[i] = 20;
    printf("\nthe safe form: a[0]=%d a[1]=%d i=%d\n", a[0], a[1], i);
    return 0;
}
