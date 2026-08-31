/* 기억의 사다리를 잰다 --- 작업 집합을 키워 가며 「한 번 읽는 데 드는 시간」을 본다.
   비결은 *포인터 추적*이다: 다음에 읽을 자리가 지금 읽은 값에 들어 있으면, 기계가
   미리 가져올 수 없고 한 번에 하나씩만 진행된다. 그래서 처리량이 아니라 *지연*이 보인다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

static double ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static uint64_t rng_state = 0x123456789ABCDEFull;
static uint64_t rnd(void)
{ rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17; return rng_state; }

/* 사톨로 알고리즘: 배열 전체를 도는 *하나의 고리*를 만든다(작은 고리로 갈라지지 않는다) */
static void make_cycle(size_t *next, size_t n)
{
    for (size_t i = 0; i < n; i++) next[i] = i;
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(rnd() % i);
        size_t t = next[i]; next[i] = next[j]; next[j] = t;
    }
    /* 순열을 고리로 바꾼다: next[a]=b 를 「a 다음은 b」로 읽게 이어 붙인다 */
    size_t *cyc = malloc(n * sizeof *cyc);
    for (size_t i = 0; i < n; i++) cyc[next[i]] = next[(i + 1) % n];
    memcpy(next, cyc, n * sizeof *cyc);
    free(cyc);
}

static volatile size_t sink;

/* 무작위 고리를 따라 steps 번 밟는다 --- 매 걸음이 앞 걸음의 결과에 의존한다 */
static double chase(size_t *next, size_t n, long steps)
{
    size_t p = 0;
    for (long i = 0; i < (long)n * 4 && i < 4000000L; i++) p = next[p];   /* 데우기 */
    double t0 = ns();
    for (long i = 0; i < steps; i++) p = next[p];
    double t1 = ns();
    sink = p;
    return (t1 - t0) / (double)steps;
}

/* 견주기용: 차례로 훑는다 --- 기계가 다음 자리를 미리 가져올 수 있다 */
static double sweep(size_t *buf, size_t n, long rounds)
{
    size_t acc = 0;
    for (size_t i = 0; i < n; i++) acc += buf[i];                    /* 데우기 */
    double t0 = ns();
    for (long r = 0; r < rounds; r++)
        for (size_t i = 0; i < n; i++) acc += buf[i];
    double t1 = ns();
    sink = acc;
    return (t1 - t0) / ((double)n * (double)rounds);
}

int main(void)
{
    const long L1 = sysconf(_SC_LEVEL1_DCACHE_SIZE);
    const long L2 = sysconf(_SC_LEVEL2_CACHE_SIZE);
    const long L3 = sysconf(_SC_LEVEL3_CACHE_SIZE);

    printf("== the caches of this machine ==\n");
    printf("  L1 %ld KiB · L2 %ld KiB · L3 %ld KiB (%.0f MiB)\n\n",
           L1 / 1024, L2 / 1024, L3 / 1024, L3 / 1048576.0);

    printf("== time for one read, by working set size ==\n");
    printf("  it follows a random ring, so prefetching does not help --- pure latency.\n\n");
    printf("  %-12s %-10s %-14s %-10s %s\n",
           "working set", "where", "random access", "factor", "sequential");

    double base = 0;
    printf("#DATA-BEGIN\n");
    for (size_t kib = 4; kib <= 131072; kib *= 2) {
        size_t bytes = kib * 1024;
        size_t n = bytes / sizeof(size_t);
        size_t *buf = malloc(bytes);
        if (!buf) { printf("  (%zu KiB allocation failed)\n", kib); break; }

        make_cycle(buf, n);

        long steps = n < 1000000 ? 4000000L : 2000000L;
        double s[5];
        for (int r = 0; r < 5; r++) s[r] = chase(buf, n, steps);
        qsort(s, 5, sizeof *s, cmp_d);
        double lat = s[2];                       /* 중앙값 */

        /* 차례로 훑기: 같은 크기를 순서대로 --- 지연이 아니라 처리량이 보인다 */
        for (size_t i = 0; i < n; i++) buf[i] = i;
        long rounds = bytes < (1u << 22) ? 200 : 5;
        double seq = sweep(buf, n, rounds);

        const char *where = (long)bytes <= L1 ? "L1"
                          : (long)bytes <= L2 ? "L2"
                          : (long)bytes <= L3 ? "L3" : "main memory";
        if (base == 0) base = lat;

        char size_s[16];
        if (kib < 1024) snprintf(size_s, sizeof size_s, "%zu KiB", kib);
        else            snprintf(size_s, sizeof size_s, "%zu MiB", kib / 1024);

        printf("  %-12s %-10s %8.2f ns %7.1fx %8.2f ns\n",
               size_s, where, lat, lat / base, seq);
        printf("#DATA %zu %.3f %.3f %s\n", kib, lat, seq, where);
        free(buf);
    }
    printf("#DATA-END\n");

    printf("\n== how to read this ==\n");
    printf("  1. reading down the table shows steps. Wherever the working set passes\n");
    printf("     the size of a cache, the cost jumps.\n");
    printf("  2. the last column (sequential) barely changes. Reading in order lets the\n");
    printf("     machine prefetch the next line --- the latency is the same, but nothing waits.\n");
    printf("  3. so \"it is fast if it fits in cache\" is only half right. More precisely:\n");
    printf("     access it unpredictably and outside the cache is tens of times slower.\n");
    return 0;
}
