/* <setjmp.h> — 저장한 자리로 되돌아가기. 흐름을 눈으로 따라간다. */
#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;

static void deep(int level)
{
    printf("  going in to depth %d\n", level);
    if (level == 3) {
        puts("  trouble at depth 3 - longjmp(env, 42)");
        longjmp(env, 42);        /* 여기서 돌아가지 않는다 */
    }
    if (level < 5) deep(level + 1);   /* 조건을 두어야 컴파일러가 무한 재귀로 보지 않는다 */
    printf("  leaving depth %d normally\n", level);  /* 도달하지 않는다 */
}

int main(void)
{
    /* setjmp 는 *두 번* 값을 내놓는 것처럼 보인다:
       ① 직접 불렸을 때 0, ② longjmp 로 돌아왔을 때 그 val */
    int rc = setjmp(env);

    if (rc == 0) {
        puts("① setjmp returned 0 - meaning the place was saved");
        deep(1);
        puts("we never get here");
    } else {
        printf("② setjmp returned %d - so we came back through longjmp\n", rc);
    }

    /* longjmp(env, 0) 을 불러도 setjmp 는 0 을 돌려주지 않는다 — 1 이 된다 */
    static int once;
    if (!once) {
        once = 1;
        puts("\ntrying longjmp(env, 0):");
        longjmp(env, 0);
    }
    printf("\nsizeof(jmp_buf) = %zu bytes\n", sizeof(jmp_buf));
    return 0;
}
