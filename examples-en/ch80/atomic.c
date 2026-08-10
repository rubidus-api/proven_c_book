/* A race condition and an atomic operation — the same work counted twice. */
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

#define THREADS 4
#define BUMPS   200000

static long        plain;   /* a plain long — unprotected */
static atomic_long safe;    /* an atomic long     */

static int worker(void *unused)
{
    (void)unused;
    for (int i = 0; i < BUMPS; i++) {
        plain = plain + 1;                    /* read-add-write: it comes apart */
        atomic_fetch_add(&safe, 1);           /* it happens as one lump      */
    }
    return 0;
}

int main(void)
{
    thrd_t t[THREADS];

    for (int i = 0; i < THREADS; i++)
        thrd_create(&t[i], worker, NULL);
    for (int i = 0; i < THREADS; i++)
        thrd_join(t[i], NULL);

    long expected = (long)THREADS * BUMPS;
    printf("expected   : %ld\n", expected);
    printf("unprotected: %ld%s\n", plain, plain == expected ? "" : "  <- counts were lost");
    printf("atomic     : %ld\n", (long)safe);

    printf("\nis atomic_long lock-free? %s\n",
           atomic_is_lock_free(&safe) ? "yes" : "no");
    return 0;
}
