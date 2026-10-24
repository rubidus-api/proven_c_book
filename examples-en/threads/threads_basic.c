/* Running in separate strands — create, wait, and watch a value go wrong. */
#include <stdio.h>
#include <stdlib.h>

#ifdef __STDC_NO_THREADS__
/* The standard says an implementation may omit this header. This file still compiles there. */
int main(void) { puts("this implementation does not provide <threads.h>"); return 0; }
#else
#include <threads.h>

/* -- A thread function has exactly one shape: int (*)(void *) ------------- */
static int greet(void *arg)
{
    const char *who = arg;
    printf("  hello from %s\n", who);
    return 7;                       /* thrd_join collects this */
}

/* -- Reproducing a race: several strands touch one cell, unprotected -------
   The volatile is NOT here to fix anything. Without it the compiler folds the
   whole loop into a single register and there is *no window for a race at all*
   (that is what happened -- nothing was lost). volatile only makes each turn a
   real load and store; it does not make the update safe. That is the next
   chapter's story. */
#define BUMPS 200000
static volatile long shared;

static int bump(void *arg)
{
    (void)arg;
    for (int i = 0; i < BUMPS; i++) shared += 1;   /* load, add, store */
    return 0;
}

int main(void)
{
    puts("[1] one thread, one return value");
    thrd_t t;
    if (thrd_create(&t, greet, "the worker") != thrd_success) {
        fputs("thrd_create failed\n", stderr);
        return EXIT_FAILURE;
    }
    int rc = 0;
    thrd_join(t, &rc);                   /* wait for it to finish and take the value */
    printf("  the worker returned %d\n\n", rc);

    puts("[2] four threads bumping one counter, unprotected");
    enum { N = 4 };
    thrd_t w[N];
    shared = 0;
    for (int i = 0; i < N; i++) thrd_create(&w[i], bump, NULL);
    for (int i = 0; i < N; i++) thrd_join(w[i], NULL);

    long expected = (long)N * BUMPS, got = shared;
    printf("  expected %ld\n", expected);
    printf("  actual   %ld%s\n", got, got == expected ? "" : "   <- updates were lost");
    puts("  (the number differs on every run --- that is what a race looks like)");
    return 0;
}
#endif
