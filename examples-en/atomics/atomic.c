/* Atomic update — four threads bump one counter, only with an indivisible operation.
   The race on an unprotected counter was shown by chapter 94's first demo. It is not
   mixed into this run: one race makes the whole run undefined, so the atomic counter
   beside it would prove nothing either. */
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#define THREADS 4
#define BUMPS   200000

static atomic_long count;   /* an atomic long */

static int worker(void *unused)
{
    (void)unused;
    for (int i = 0; i < BUMPS; i++)
        atomic_fetch_add(&count, 1);          /* read-add-write happens as one lump */
    return 0;
}

int main(void)
{
    thrd_t t[THREADS];
    int made = 0;

    for (; made < THREADS; made++)
        if (thrd_create(&t[made], worker, NULL) != thrd_success)
            break;
    /* wait for every thread that was made, failure or not */
    bool joined = true;
    for (int i = 0; i < made; i++)
        if (thrd_join(t[i], NULL) != thrd_success)
            joined = false;
    if (made < THREADS || !joined) {
        fprintf(stderr, "created %d of %d threads, joined all: %s\n",
                made, THREADS, joined ? "yes" : "no");
        return EXIT_FAILURE;
    }

    /* reaching here, all four threads were made and all have finished --- only then is 800,000 guaranteed */
    printf("expected: %ld\n", (long)THREADS * BUMPS);
    printf("atomic  : %ld\n", (long)atomic_load(&count));

    printf("\nis atomic_long lock-free? %s\n",
           atomic_is_lock_free(&count) ? "yes" : "no");
    return 0;
}
