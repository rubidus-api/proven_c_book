/* 코어 사이의 값 --- 같은 캐시 줄을 두 코어가 번갈아 만지면 무슨 일이 벌어지나.
   그리고 원자 연산과 자물쇠는 얼마나 드는가. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

static double ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

#define ITERS 20000000L        /* 스레드마다 2천만 번 */
#define ROUNDS 3

static long line_size;

/* ★ aarch64 리눅스는 CTR_EL0 레지스터를 사용자 프로그램에도 읽게 열어 둔다(glibc 도 이것으로
     줄 크기를 답한다). 안드로이드의 Bionic 은 sysconf 에 0 을 돌려주고, 폰 커널은 sysfs 의 크기
     칸을 비워 두기도 해서(안드로이드 폰 실측) 마지막으로 이 레지스터를 직접 읽는다.
     다만 계층 전체에서 *가장 작은* 줄 크기다. */
#if defined(__aarch64__) && defined(__linux__)
__asm__(".text\n.globl read_ctr_el0\n.type read_ctr_el0, %function\n"
        "read_ctr_el0:\n    mrs x0, ctr_el0\n    ret\n");
unsigned long read_ctr_el0(void);
#endif

static long cache_line(void)
{
    long v = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (v > 0)
        return v;
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if (f) {
        if (fscanf(f, "%ld", &v) != 1)
            v = 0;
        fclose(f);
        if (v > 0)
            return v;
    }
#if defined(__aarch64__) && defined(__linux__)
    return 4L << ((read_ctr_el0() >> 16) & 0xf);
#else
    return 0;                                  /* 모른다 --- 부르는 쪽이 밝힌다 */
#endif
}

/* ★ aligned_alloc(C11) 대신 posix_memalign 을 쓴다. _POSIX_C_SOURCE 를 정의하면 안드로이드의
     Bionic 은 C11 선언을 감춘다 --- Termux 의 clang 에서 「선언되지 않은 함수」로 멈췄다. */
static void *page_aligned(size_t bytes)
{
    void *p = NULL;
    return posix_memalign(&p, 4096, bytes) == 0 ? p : NULL;
}

/* ── ① 각자 제 칸을 올린다 (칸 사이의 거리를 바꿔 가며) ── */
struct slot { volatile long v; };
static unsigned char *arena;
static size_t slot_gap;

static void *bump(void *arg)
{
    long idx = (long)(intptr_t)arg;
    volatile long *p = (volatile long *)(arena + (size_t)idx * slot_gap);
    for (long i = 0; i < ITERS; i++) (*p)++;
    return NULL;
}

/* ── ② 하나의 값을 여럿이 --- 원자 연산과 자물쇠 ── */
static atomic_long shared_atomic;
static long shared_plain;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void *atomic_bump(void *arg)
{ (void)arg; for (long i = 0; i < ITERS / 10; i++) atomic_fetch_add_explicit(&shared_atomic, 1, memory_order_relaxed); return NULL; }
static void *mutex_bump(void *arg)
{ (void)arg; for (long i = 0; i < ITERS / 100; i++) { pthread_mutex_lock(&lock); shared_plain++; pthread_mutex_unlock(&lock); } return NULL; }
static double run(void *(*fn)(void *), int threads, long iters_each, void **args)
{
    pthread_t th[64];
    double t0 = ns();
    for (int i = 0; i < threads; i++) pthread_create(&th[i], NULL, fn, args ? args[i] : (void *)(intptr_t)i);
    for (int i = 0; i < threads; i++) pthread_join(th[i], NULL);
    double t1 = ns();
    return (t1 - t0) / (double)(iters_each * threads);     /* 올림 한 번당 나노초 */
}

int main(void)
{
    line_size = cache_line();
    if (line_size <= 0) {
        line_size = 64;
        printf("(this system does not report its cache line --- 64 bytes is assumed below)\n");
    }
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    printf("== this machine ==\n  cache line %ld bytes · %ld logical cores\n\n", line_size, cores);

    /* 기준선: *같은 코드*를 스레드 하나로. 지역 변수 루프는 컴파일러가 통째로 접어
       버려(s = ITERS) 0.005 나노초 같은 헛값이 나온다 --- M1 의 첫 함정이다.
       그래서 뒤의 실험과 똑같은 경로(volatile 칸 올리기)를 한 스레드로 돌려 기준을 잡는다. */
    slot_gap = 128;
    arena = page_aligned(slot_gap * 8 + 4096);
    memset(arena, 0, slot_gap * 8 + 4096);
    double bs[ROUNDS];
    for (int r = 0; r < ROUNDS; r++) bs[r] = run(bump, 1, ITERS, NULL);
    qsort(bs, ROUNDS, sizeof *bs, cmp_d);
    double base = bs[ROUNDS / 2];
    free(arena);
    printf("== 1. the baseline ==\n");
    printf("  one thread incrementing its own slot : %.3f ns each\n", base);
    printf("  (measured on a local variable the compiler folds the loop away, so it is no baseline)\n\n");

    /* 칸 사이의 거리를 바꿔 가며 두 스레드 */
    printf("== 2. two threads each incrementing their own slot --- only the distance changes ==\n");
    printf("  the two threads never touch each other's data. And yet…\n\n");
    printf("  with two threads, a perfect split would make each increment half the\n");
    printf("  baseline (%.3f ns). Anything above that is the loss.\n\n", base / 2);
    printf("  %-14s %-16s %-14s %s\n", "distance", "per increment", "vs perfect scaling", "same line?");
    printf("#DATA-BEGIN\n");
    for (size_t gap = 8; gap <= 256; gap *= 2) {
        slot_gap = gap;
        arena = page_aligned(gap * 8 + 4096);
        memset(arena, 0, gap * 8 + 4096);
        double s[ROUNDS];
        for (int r = 0; r < ROUNDS; r++) s[r] = run(bump, 2, ITERS, NULL);
        qsort(s, ROUNDS, sizeof *s, cmp_d);
        printf("  %-14zu %10.3f ns %10.2fx  %s\n", gap, s[ROUNDS / 2],
               s[ROUNDS / 2] / (base / 2),
               (long)gap < line_size ? "same line --- false sharing" : "different lines");
        printf("#DATA %zu %.3f\n", gap, s[ROUNDS / 2]);
        free(arena);
    }
    printf("#DATA-END\n");
    printf("\n  * the two threads touch different variables. But in the same cache line the\n");
    printf("    line is passed back and forth between the cores --- that is false sharing.\n");
    printf("    Moving them apart changes it several fold, with not one line of code altered.\n");

    /* 스레드 수를 늘리면 */
    printf("\n== 3. adding threads --- sharing a line, and apart ==\n");
    printf("  %-10s %-18s %-18s %s\n", "threads", "same line (8 bytes apart)", "different lines (128 bytes)", "factor");
    for (int th = 2; th <= (cores >= 8 ? 8 : 4); th *= 2) {
        double v[2];
        for (int k = 0; k < 2; k++) {
            slot_gap = k ? 128 : 8;
            arena = page_aligned(slot_gap * 16 + 4096);
            memset(arena, 0, slot_gap * 16 + 4096);
            double s[ROUNDS];
            for (int r = 0; r < ROUNDS; r++) s[r] = run(bump, th, ITERS, NULL);
            qsort(s, ROUNDS, sizeof *s, cmp_d);
            v[k] = s[ROUNDS / 2];
            free(arena);
        }
        printf("  %-10d %12.3f ns %12.3f ns %6.1fx\n", th, v[0], v[1], v[0] / v[1]);
    }

    /* 진짜로 함께 쓰는 값 */
    printf("\n== 4. when several really do touch one value ==\n");
    double at1 = run(atomic_bump, 1, ITERS / 10, NULL);
    double at2 = run(atomic_bump, 2, ITERS / 10, NULL);
    double at4 = run(atomic_bump, 4, ITERS / 10, NULL);
    double mx1 = run(mutex_bump, 1, ITERS / 100, NULL);
    double mx2 = run(mutex_bump, 2, ITERS / 100, NULL);
    printf("  %-34s %10.3f ns each\n", "uncontended increment (baseline)", base);
    printf("  %-34s %10.3f ns each\n", "atomic increment --- 1 thread", at1);
    printf("  %-34s %10.3f ns each\n", "atomic increment --- 2 threads", at2);
    printf("  %-34s %10.3f ns each\n", "atomic increment --- 4 threads", at4);
    printf("  %-34s %10.3f ns each\n", "increment under a lock --- 1 thread", mx1);
    printf("  %-34s %10.3f ns each\n", "increment under a lock --- 2 threads", mx2);
    printf("\n  * uncontended (one thread), the atomic costs %.0fx and the lock %.0fx.\n",
           at1 / base, mx1 / base);
    printf("    But when several contend for one value it jumps several fold again --- one\n");
    printf("    line has to be owned exclusively by each core in turn.\n");
    printf("  * hence the discipline: count separately, and combine once at the end.\n");
    return 0;
}
