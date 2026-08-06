/* <setjmp.h> — 저장한 자리로 되돌아가기. 흐름을 눈으로 따라간다. */
#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;

static void deep(int level)
{
    printf("  깊이 %d 로 들어간다\n", level);
    if (level == 3) {
        puts("  깊이 3 에서 문제 발생 — longjmp(env, 42)");
        longjmp(env, 42);        /* 여기서 돌아가지 않는다 */
    }
    if (level < 5) deep(level + 1);   /* 조건을 두어야 컴파일러가 무한 재귀로 보지 않는다 */
    printf("  깊이 %d 를 정상으로 빠져나온다\n", level);  /* 도달하지 않는다 */
}

int main(void)
{
    /* setjmp 는 *두 번* 값을 내놓는 것처럼 보인다:
       ① 직접 불렸을 때 0, ② longjmp 로 돌아왔을 때 그 val */
    int rc = setjmp(env);

    if (rc == 0) {
        puts("① setjmp 가 0 을 돌려주었다 — 자리를 저장했다는 뜻");
        deep(1);
        puts("여기는 실행되지 않는다");
    } else {
        printf("② setjmp 가 %d 를 돌려주었다 — longjmp 로 돌아온 것\n", rc);
    }

    /* longjmp(env, 0) 을 불러도 setjmp 는 0 을 돌려주지 않는다 — 1 이 된다 */
    static int once;
    if (!once) {
        once = 1;
        puts("\nlongjmp(env, 0) 을 시험한다:");
        longjmp(env, 0);
    }
    printf("\njmp_buf 의 크기 = %zu바이트\n", sizeof(jmp_buf));
    return 0;
}
