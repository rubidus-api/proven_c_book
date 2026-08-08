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
        puts("셋 다 2 로 바꾼 뒤 longjmp 한다");
        fail();
    }

    printf("\nlongjmp 로 돌아온 뒤:\n");
    printf("  plain   (자동, 비 volatile) = %d   ← 표준은 값을 보장하지 않는다\n",
           plain);
    printf("  guarded (자동, volatile)    = %d   ← 보장된다\n", guarded);
    printf("  statik  (정적)              = %d   ← 보장된다\n", statik);

#ifdef __OPTIMIZE__
    puts("\n(최적화가 켜진 빌드다. plain 이 1 로 되돌아간 것을 보라 —");
    puts(" 컴파일러가 그 변수를 레지스터에 두었기 때문이다.)");
#else
    puts("\n(최적화가 꺼진 빌드다. plain 도 2 로 보이지만 보장이 아니다 —");
    puts(" 같은 코드를 -O2 로 빌드하면 1 이 나온다. 직접 확인했다.)");
#endif
    puts("규칙은 하나다: longjmp 를 건너 살아남아야 하는 지역 변수에는");
    puts("volatile 을 붙인다.");
    return 0;
}
