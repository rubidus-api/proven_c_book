/* longjmp 로 돌아왔을 때 지역 변수는 어떻게 되는가 — volatile 이 필요한 이유. */
#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;

static void fail(void) { longjmp(env, 1); }

int main(void)
{
    /* 표준의 규칙(§7.13.2.1): setjmp 를 부른 함수의 자동 변수 중
       ① volatile 이 아니고
       ② setjmp 와 longjmp 사이에 값이 바뀐 것
       은 longjmp 로 돌아왔을 때 *표현이 정해지지 않는다*.
       그 밖의 모든 객체는 longjmp 를 부른 시점의 값을 그대로 가진다. */
    int                 plain    = 1;    /* 위험: 자동 + 비 volatile */
    volatile int        guarded  = 1;    /* 안전: volatile */
    static int          statik   = 1;    /* 안전: 정적 저장 기간 */

    if (setjmp(env) == 0) {
        plain   = 2;                     /* setjmp 와 longjmp 사이에서 바꾼다 */
        guarded = 2;
        statik  = 2;
        puts("setting all three to 2, then longjmp");
        fail();
    }

    printf("\nafter coming back through longjmp:\n");
    printf("  plain   (automatic, not volatile) = %d   <- the standard guarantees nothing\n",
           plain);
    printf("  guarded (automatic, volatile)     = %d   <- guaranteed\n", guarded);
    printf("  statik  (static)                  = %d   <- guaranteed\n", statik);

#ifdef __OPTIMIZE__
    puts("\n(this build has optimization on. Notice that plain went back to 1 -");
    puts(" the compiler had kept that variable in a register.)");
#else
    puts("\n(this build has optimization off. plain reads 2 as well, but that is not guaranteed -");
    puts(" build the same code with -O2 and it prints 1. We checked.)");
#endif
    puts("the rule is one line: a local that must survive a longjmp gets");
    puts("volatile.");
    return 0;
}
