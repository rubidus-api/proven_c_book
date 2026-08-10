/* Sleeping and waking — mutual exclusion, condition variables, once, per-strand. */
#include <stdio.h>
#include <stdlib.h>

#ifdef __STDC_NO_THREADS__
int main(void) { puts("this implementation does not provide <threads.h>"); return 0; }
#else
#include <threads.h>

/* -- 1: mutual exclusion -- the previous race, stopped by a lock ---------- */
#define BUMPS 200000
static long guarded;                 /* no volatile needed -- the lock gives the ordering */
static mtx_t lock;

static int bump_guarded(void *arg)
{
    (void)arg;
    for (int i = 0; i < BUMPS; i++) {
        mtx_lock(&lock);
        guarded += 1;
        mtx_unlock(&lock);
    }
    return 0;
}

/* -- 2: a condition variable -- "wait until there is something" ----------- */
static mtx_t qlock;
static cnd_t not_empty;
static int items;                    /* the smallest possible queue: just a count */
static bool closed;

static int consumer(void *arg)
{
    int *taken = arg;
    mtx_lock(&qlock);
    for (;;) {
        /* while, not if -- waking up does not mean the condition holds */
        while (items == 0 && !closed)
            cnd_wait(&not_empty, &qlock);
        if (items == 0 && closed) break;
        items -= 1;
        *taken += 1;
    }
    mtx_unlock(&qlock);
    return 0;
}

/* -- 3: once -- lazy initialization --------------------------------------- */
static once_flag once = ONCE_FLAG_INIT;
static int init_count;

static void init_table(void) { init_count += 1; }

static int touch(void *arg)
{
    (void)arg;
    call_once(&once, init_table);
    return 0;
}

/* -- 4: one copy per strand ------------------------------------------------ */
static thread_local int mine;        /* a C23 keyword (chapter 82) */
static int per_thread(void *arg)
{
    mine = *(int *)arg;              /* each strand has its own */
    return mine;
}

int main(void)
{
    enum { N = 4 };
    thrd_t w[N];

    puts("[1] the same counter, now guarded by a mutex");
    mtx_init(&lock, mtx_plain);
    guarded = 0;
    for (int i = 0; i < N; i++) thrd_create(&w[i], bump_guarded, NULL);
    for (int i = 0; i < N; i++) thrd_join(w[i], NULL);
    printf("  expected %ld, actual %ld%s\n", (long)N * BUMPS, guarded,
           guarded == (long)N * BUMPS ? "   <- nothing lost, every run" : "   <- lost");
    mtx_destroy(&lock);

    puts("\n[2] a condition variable: wait until there is something");
    mtx_init(&qlock, mtx_plain);
    cnd_init(&not_empty);
    int taken = 0;
    thrd_t c;
    thrd_create(&c, consumer, &taken);
    for (int i = 0; i < 5; i++) {
        mtx_lock(&qlock);
        items += 1;
        cnd_signal(&not_empty);       /* signal while holding the lock */
        mtx_unlock(&qlock);
    }
    mtx_lock(&qlock);
    closed = true;
    cnd_broadcast(&not_empty);        /* tell everyone we are done */
    mtx_unlock(&qlock);
    thrd_join(c, NULL);
    printf("  produced 5, consumer took %d\n", taken);
    cnd_destroy(&not_empty);
    mtx_destroy(&qlock);

    puts("\n[3] call_once: four threads, one initialization");
    for (int i = 0; i < N; i++) thrd_create(&w[i], touch, NULL);
    for (int i = 0; i < N; i++) thrd_join(w[i], NULL);
    printf("  init ran %d time(s)\n", init_count);

    puts("\n[4] thread_local: each thread has its own");
    int vals[N];
    int got[N];
    for (int i = 0; i < N; i++) { vals[i] = (i + 1) * 10; thrd_create(&w[i], per_thread, &vals[i]); }
    for (int i = 0; i < N; i++) thrd_join(w[i], &got[i]);
    printf("  each thread saw:");
    for (int i = 0; i < N; i++) printf(" %d", got[i]);
    printf("\n  main's own copy is still %d\n", mine);
    return 0;
}
#endif
