/* <setjmp.h> — going back to a saved place. Follow the flow with your eyes. */
#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;

static void deep(int level)
{
    printf("  entering depth %d\n", level);
    if (level == 3) {
        puts("  trouble at depth 3 — longjmp(env, 42)");
        longjmp(env, 42);        /* does not return from here */
    }
    if (level < 5) deep(level + 1);   /* the guard keeps the compiler from seeing infinite recursion */
    printf("  leaving depth %d normally\n", level);      /* not reached */
}

int main(void)
{
    /* setjmp appears to yield a value *twice*:
       (1) 0 when called directly, (2) the val when returned to by longjmp */
    int rc = setjmp(env);

    if (rc == 0) {
        puts("(1) setjmp returned 0 — meaning the place was saved");
        deep(1);
        puts("this line does not run");
    } else {
        printf("(2) setjmp returned %d — we came back by longjmp\n", rc);
    }

    /* even longjmp(env, 0) does not make setjmp return 0 — it becomes 1 */
    static int once;
    if (!once) {
        once = 1;
        puts("\ntesting longjmp(env, 0):");
        longjmp(env, 0);
    }
    printf("\nsizeof(jmp_buf) = %zu bytes\n", sizeof(jmp_buf));
    return 0;
}
