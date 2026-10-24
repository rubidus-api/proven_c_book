/* when a pointer reverts after a longjmp — the road to leaks and double frees. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

static jmp_buf  env;
static void    *recorded;       /* the address malloc really returned (static, so safe) */

[[noreturn]] static void fail(void) { longjmp(env, 1); }

int main(void)
{
    char          *risky = NULL;   /* automatic + non-volatile — the risky place */
    char *volatile safe  = NULL;   /* the pointer itself is volatile */

    if (setjmp(env) == 0) {
        risky    = malloc(64);
        safe     = risky;
        recorded = risky;
        if (!risky) return 1;
        puts("took 64 bytes with malloc. then something deep fails.");
        fail();
    }

    /* back again. do the two variables still hold the same address? */
    printf("does risky hold the allocated address? %s\n",
           (void *)risky == recorded ? "yes" : "no - it reverted to the old value");
    printf("does safe  hold the allocated address? %s\n",
           (void *)safe  == recorded ? "yes" : "no - it reverted to the old value");

#ifdef __OPTIMIZE__
    puts("\n(This build has optimization on. See risky revert to the null it held");
    puts(" at setjmp: free(risky) frees null, does nothing, and 64 bytes leak.)");
#else
    puts("\n(This build has optimization off. Both survived, but risky is not");
    puts(" guaranteed - built with -O2 it reverts to null. We checked.)");
#endif

    free((void *)safe);            /* free through the volatile one — the only one inside the contract */
    puts("freed exactly once, through safe.");
    return 0;
}
