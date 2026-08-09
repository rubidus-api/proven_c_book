/* See what a storage-class specifier decides.
   Also the rule that one declaration takes only one of them - and its
   single exception. */
#include <stdio.h>

static int file_only = 1;          /* internal linkage - visible only in this file */
extern int shared;                 /* a promise only - the real thing is below */
int        shared = 2;             /* the definition */
thread_local int per_thread = 3;   /* one per thread */

static int bump(void)
{
    static int calls = 0;          /* static storage duration - survives between calls */
    return ++calls;
}

int main(void)
{
    auto int local = 4;            /* the pre-C23 meaning: automatic storage duration (the default) */
    register int hot = 5;          /* the old request. its address cannot be taken */
    constexpr int fixed = 6;       /* C23 - a compile-time constant */
    static_assert(fixed == 6, "constexpr is a constant expression");

    printf("file_only=%d shared=%d per_thread=%d local=%d hot=%d fixed=%d\n",
           file_only, shared, per_thread, local, hot, fixed);
    /* Calling it several times in one statement leaves the order unspecified
       (chapter 34), so call it once per line. */
    int b1 = bump(), b2 = bump(), b3 = bump();
    printf("bump() three times: %d %d %d   <- a static local survives\n", b1, b2, b3);

    /* static extern int bad;   <- error: multiple storage classes            */
    /* int *p = &hot;           <- error: address of register variable        */
    /* auto constexpr int c=1;  <- error: 'auto' used with 'constexpr'        */
    static thread_local int ok = 7;   /* the one combination allowed as an exception */
    printf("static thread_local ok=%d\n", ok);
    return 0;
}
