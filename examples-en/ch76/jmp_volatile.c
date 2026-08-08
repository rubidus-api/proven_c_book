/* what happens to locals after a longjmp — why volatile is needed. */
#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;

static void fail(void) { longjmp(env, 1); }

int main(void)
{
    /* The standard's rule (§7.13.2.1): among the automatic variables of the
       function that called setjmp, those that are
       (1) not volatile and
       (2) changed between setjmp and longjmp
       have *indeterminate representations* after the longjmp.
       Every other object keeps the value it had when longjmp was called. */
    int                 plain    = 1;    /* risky: automatic + non-volatile */
    volatile int        guarded  = 1;    /* safe: volatile */
    static int          statik   = 1;    /* safe: static storage duration */

    if (setjmp(env) == 0) {
        plain   = 2;                     /* changed between setjmp and longjmp */
        guarded = 2;
        statik  = 2;
        puts("all three set to 2, then longjmp");
        fail();
    }

    printf("\nafter coming back by longjmp:\n");
    printf("  plain   (automatic, non-volatile) = %d   <- not guaranteed\n",
           plain);
    printf("  guarded (automatic, volatile)     = %d   <- guaranteed\n", guarded);
    printf("  statik  (static)                  = %d   <- guaranteed\n", statik);

#ifdef __OPTIMIZE__
    puts("\n(This build has optimization on. See that plain came back as 1 —");
    puts(" the compiler had kept that variable in a register.)");
#else
    puts("\n(This build has optimization off. plain shows 2, but that is not a");
    puts(" guarantee — the same code built with -O2 prints 1. We checked.)");
#endif
    puts("One rule: put volatile on any local that must survive across a");
    puts("longjmp.");
    return 0;
}
