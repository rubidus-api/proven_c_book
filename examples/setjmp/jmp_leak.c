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
        puts("64 bytes taken with malloc. Then something fails deep down.");
        fail();
    }

    /* 되돌아왔다. 두 변수가 같은 주소를 가리키고 있는가? */
    printf("is risky still the real allocation address? %s\n",
           (void *)risky == recorded ? "yes" : "no - it snapped back to the old value");
    printf("is safe  still the real allocation address? %s\n",
           (void *)safe  == recorded ? "yes" : "no - it snapped back to the old value");

#ifdef __OPTIMIZE__
    puts("\n(this build has optimization on. Notice that risky went back to the null it held at setjmp.");
    puts(" free(risky) then frees a null pointer, does nothing, and the 64 bytes leak for good.)");
#else
    puts("\n(this build has optimization off. Both survived, but risky is not guaranteed to -");
    puts(" build the same code with -O2 and risky goes back to null. We checked.)");
#endif

    free((void *)safe);            /* volatile 쪽으로 푼다 — 이것만이 계약 안 */
    puts("freed exactly once with free(safe).");
    return 0;
}
