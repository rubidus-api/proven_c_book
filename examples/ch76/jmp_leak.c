/* longjmp 뒤에 포인터가 옛 값으로 되돌아가면 — 누수와 이중 해제의 경로. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

static jmp_buf  env;
static void    *recorded;       /* malloc 이 실제로 돌려준 주소(정적이라 안전) */

[[noreturn]] static void fail(void) { longjmp(env, 1); }

int main(void)
{
    char          *risky = NULL;   /* 자동 + 비 volatile — 위험한 자리 */
    char *volatile safe  = NULL;   /* 포인터 자체를 volatile 로 */

    if (setjmp(env) == 0) {
        risky    = malloc(64);
        safe     = risky;
        recorded = risky;
        if (!risky) return 1;
        puts("malloc 으로 64바이트를 잡았다. 그리고 깊은 곳에서 실패한다.");
        fail();
    }

    /* 되돌아왔다. 두 변수가 같은 주소를 가리키고 있는가? */
    printf("risky 가 실제 할당 주소인가? %s\n",
           (void *)risky == recorded ? "예" : "아니오 — 옛 값으로 되돌아갔다");
    printf("safe  가 실제 할당 주소인가? %s\n",
           (void *)safe  == recorded ? "예" : "아니오 — 옛 값으로 되돌아갔다");

#ifdef __OPTIMIZE__
    puts("\n(최적화가 켜진 빌드다. risky 가 setjmp 시점의 널로 되돌아간 것을 보라.");
    puts(" free(risky) 는 널을 풀어 아무 일도 하지 않고, 64바이트는 영영 샌다.)");
#else
    puts("\n(최적화가 꺼진 빌드다. 둘 다 살아남았지만 risky 쪽은 보장이 아니다 —");
    puts(" 같은 코드를 -O2 로 빌드하면 risky 가 널로 되돌아간다. 직접 확인했다.)");
#endif

    free((void *)safe);            /* volatile 쪽으로 푼다 — 이것만이 계약 안 */
    puts("free(safe) 로 정확히 한 번 풀었다.");
    return 0;
}
