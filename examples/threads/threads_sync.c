/* 재우고 깨우기 — 상호 배제, 조건 변수, 한 번만, 갈래마다 따로. */
#include <stdio.h>
#include <stdlib.h>

#ifdef __STDC_NO_THREADS__
int main(void) { puts("this implementation does not provide <threads.h>"); return 0; }
#else
#include <threads.h>

/* 만들기 · 기다리기 · 준비 가운데 하나라도 실패하면 여기서 멈춘다.
   아래에 찍는 「기대한 값」은 모두 성공했을 때의 이야기다. */
static void must(int rc, const char *what)
{
    if (rc != thrd_success) {
        fprintf(stderr, "%s failed\n", what);
        exit(EXIT_FAILURE);
    }
}

/* ── ① 상호 배제 — 앞 예제의 경쟁을 잠금으로 막는다 ───────────── */
#define BUMPS 200000
static long guarded;                 /* volatile 이 필요 없다 — 잠금이 순서를 준다 */
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

/* ── ② 조건 변수 — 「생길 때까지 기다린다」 ────────────────────── */
static mtx_t qlock;
static cnd_t not_empty;
static int items;                    /* 아주 작은 큐: 개수만 센다 */
static bool closed;

static int consumer(void *arg)
{
    int *taken = arg;
    mtx_lock(&qlock);
    for (;;) {
        /* ★ while 이지 if 가 아니다 — 깨어났다고 조건이 참인 것은 아니다 */
        while (items == 0 && !closed)
            cnd_wait(&not_empty, &qlock);
        if (items == 0 && closed) break;
        items -= 1;
        *taken += 1;
    }
    mtx_unlock(&qlock);
    return 0;
}

/* ── ③ 한 번만 — 게으른 초기화 ────────────────────────────────── */
static once_flag once = ONCE_FLAG_INIT;
static int init_count;

static void init_table(void) { init_count += 1; }

static int touch(void *arg)
{
    (void)arg;
    call_once(&once, init_table);
    return 0;
}

/* ── ④ 갈래마다 따로 ──────────────────────────────────────────── */
static thread_local int mine;        /* C23 의 키워드 (97장) */
static int per_thread(void *arg)
{
    mine = *(int *)arg;              /* 갈래마다 제 몫이 따로 있다 */
    return mine;
}

int main(void)
{
    enum { N = 4 };
    thrd_t w[N];

    puts("[1] the same counter, now guarded by a mutex");
    must(mtx_init(&lock, mtx_plain), "mtx_init");
    guarded = 0;
    for (int i = 0; i < N; i++) must(thrd_create(&w[i], bump_guarded, NULL), "thrd_create");
    for (int i = 0; i < N; i++) must(thrd_join(w[i], NULL), "thrd_join");
    printf("  expected %ld, actual %ld%s\n", (long)N * BUMPS, guarded,
           guarded == (long)N * BUMPS ? "   <- nothing lost (the mutex guarantees it)" : "   <- lost");
    mtx_destroy(&lock);

    puts("\n[2] a condition variable: wait until there is something");
    must(mtx_init(&qlock, mtx_plain), "mtx_init");
    must(cnd_init(&not_empty), "cnd_init");
    int taken = 0;
    thrd_t c;
    must(thrd_create(&c, consumer, &taken), "thrd_create");
    for (int i = 0; i < 5; i++) {
        mtx_lock(&qlock);
        items += 1;
        cnd_signal(&not_empty);       /* 잠금을 쥔 채 알린다 */
        mtx_unlock(&qlock);
    }
    mtx_lock(&qlock);
    closed = true;
    cnd_broadcast(&not_empty);        /* 끝났음을 모두에게 */
    mtx_unlock(&qlock);
    must(thrd_join(c, NULL), "thrd_join");
    printf("  produced 5, consumer took %d\n", taken);
    cnd_destroy(&not_empty);
    mtx_destroy(&qlock);

    puts("\n[3] call_once: four threads, one initialization");
    for (int i = 0; i < N; i++) must(thrd_create(&w[i], touch, NULL), "thrd_create");
    for (int i = 0; i < N; i++) must(thrd_join(w[i], NULL), "thrd_join");
    printf("  init ran %d time(s)\n", init_count);

    puts("\n[4] thread_local: each thread has its own");
    int vals[N];
    int got[N];
    for (int i = 0; i < N; i++) { vals[i] = (i + 1) * 10; must(thrd_create(&w[i], per_thread, &vals[i]), "thrd_create"); }
    for (int i = 0; i < N; i++) must(thrd_join(w[i], &got[i]), "thrd_join");
    printf("  each thread saw:");
    for (int i = 0; i < N; i++) printf(" %d", got[i]);
    printf("\n  main's own copy is still %d\n", mine);
    return 0;
}
#endif
