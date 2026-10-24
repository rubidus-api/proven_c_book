/* 갈래를 나누어 돌리기 — 만들고, 기다리고, 그리고 값이 어긋나는 것을 본다. */
#include <stdio.h>
#include <stdlib.h>

#ifdef __STDC_NO_THREADS__
/* 표준이 「없어도 적합」이라고 적어 둔 헤더다. 없는 구현에서도 이 파일은 컴파일된다. */
int main(void) { puts("this implementation does not provide <threads.h>"); return 0; }
#else
#include <threads.h>

/* ── 스레드 함수의 꼴은 하나뿐이다: int (*)(void *) ───────────── */
static int greet(void *arg)
{
    const char *who = arg;
    printf("  hello from %s\n", who);
    return 7;                       /* 이 값은 thrd_join 이 받아 간다 */
}

/* ── 경쟁을 재현한다 — 보호 없이 같은 칸을 만진다 ─────────────────
   ★ volatile 을 붙인 이유는 「고치려고」가 아니다. 붙이지 않으면 컴파일러가
   루프 전체를 레지스터 하나로 접어 버려서 *경쟁이 일어날 틈이 없어진다*
   (실제로 그렇게 되어 아무것도 잃지 않았다). volatile 은 매 반복을 진짜
   읽기-쓰기로 만들 뿐, 갱신을 안전하게 하지 않는다 — 다음 장이 그 이야기다. */
#define BUMPS 200000
static volatile long shared;

static int bump(void *arg)
{
    (void)arg;
    for (int i = 0; i < BUMPS; i++) shared += 1;   /* 읽고 · 더하고 · 쓴다 */
    return 0;
}

int main(void)
{
    puts("[1] one thread, one return value");
    thrd_t t;
    if (thrd_create(&t, greet, "the worker") != thrd_success) {
        fputs("thrd_create failed\n", stderr);
        return EXIT_FAILURE;
    }
    int rc = 0;
    thrd_join(t, &rc);                   /* 끝날 때까지 기다리고 값을 받는다 */
    printf("  the worker returned %d\n\n", rc);

    puts("[2] four threads bumping one counter, unprotected");
    enum { N = 4 };
    thrd_t w[N];
    shared = 0;
    for (int i = 0; i < N; i++) thrd_create(&w[i], bump, NULL);
    for (int i = 0; i < N; i++) thrd_join(w[i], NULL);

    long expected = (long)N * BUMPS, got = shared;
    printf("  expected %ld\n", expected);
    printf("  actual   %ld%s\n", got, got == expected ? "" : "   <- updates were lost");
    puts("  (the number differs on every run --- that is what a race looks like)");
    return 0;
}
#endif
