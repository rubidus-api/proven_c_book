/* 재기 전에 *재는 도구*를 잰다.
   시계의 해상도, 시계를 읽는 값, 최적화가 코드를 지우는 것, 데우기, 그리고 평균의 함정. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

/* 최적화가 지워도 되는 계산: 결과를 아무도 안 쓴다 */
static void sum_discarded(const int *a, size_t n)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) s += a[i];
    (void)s;
}
/* 최적화가 지울 수 없는 계산: 결과를 돌려주고, 부르는 쪽이 쓴다 */
static long sum_kept(const int *a, size_t n)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) s += a[i];
    return s;
}
static volatile long sink;

int main(void)
{
    printf("== 1. this machine's numbers (the example reads them itself) ==\n");
    printf("  %-22s %s\n", "L1 data cache", "");
    printf("    size        : %ld bytes (%ld KiB)\n",
           sysconf(_SC_LEVEL1_DCACHE_SIZE), sysconf(_SC_LEVEL1_DCACHE_SIZE) / 1024);
    printf("    line size   : %ld bytes\n", sysconf(_SC_LEVEL1_DCACHE_LINESIZE));
    printf("    associativity: %ld-way\n", sysconf(_SC_LEVEL1_DCACHE_ASSOC));
    printf("  L2 cache      : %ld KiB\n", sysconf(_SC_LEVEL2_CACHE_SIZE) / 1024);
    printf("  L3 cache      : %ld KiB (%.0f MiB)\n", sysconf(_SC_LEVEL3_CACHE_SIZE) / 1024,
           sysconf(_SC_LEVEL3_CACHE_SIZE) / 1048576.0);
    printf("  page size     : %ld bytes\n", sysconf(_SC_PAGESIZE));
    printf("  logical cores : %ld\n\n", sysconf(_SC_NPROCESSORS_ONLN));

    printf("== 2. clock resolution --- how finely can it be seen ==\n");
    struct { const char *name; clockid_t id; } clocks[] = {
        { "CLOCK_MONOTONIC   (never goes backwards)", CLOCK_MONOTONIC },
        { "CLOCK_REALTIME    (wall clock --- it jumps when adjusted)", CLOCK_REALTIME },
        { "CLOCK_PROCESS_CPUTIME_ID (CPU time I used)", CLOCK_PROCESS_CPUTIME_ID },
    };
    for (unsigned i = 0; i < sizeof clocks / sizeof *clocks; i++) {
        struct timespec r;
        clock_getres(clocks[i].id, &r);
        printf("  %-42s resolution %ld ns\n", clocks[i].name,
               (long)(r.tv_sec * 1000000000L + r.tv_nsec));
    }
    printf("  * a resolution of 1 ns does not mean 1 ns can be measured.\n");
    printf("    Reading the clock costs more than that --- measured next.\n\n");

    printf("== 3. the cost of reading the clock ==\n");
    const long N = 200000;
    double t0 = ns();
    for (long i = 0; i < N; i++) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); sink += ts.tv_nsec; }
    double t1 = ns();
    double call_ns = (t1 - t0) / (double)N;
    printf("  one clock_gettime : %.1f ns\n", call_ns);

    /* 실제로 볼 수 있는 가장 짧은 간격 --- 두 번 읽어 차이가 0 이 아닌 최솟값 */
    double min_gap = 1e18;
    for (long i = 0; i < 100000; i++) {
        double a = ns(), b = ns();
        if (b - a > 0 && b - a < min_gap) min_gap = b - a;
    }
    printf("  smallest interval actually distinguishable : %.1f ns\n", min_gap);
    printf("  * so anything shorter than this is never measured once. Repeat and divide.\n");
    printf("    Every measurement in this appendix is built that way.\n\n");

    printf("== 4. trap one: the optimiser removes what you meant to measure ==\n");
    const size_t NA = 1u << 20;                 /* 4 MiB 짜리 배열 */
    int *a = malloc(NA * sizeof *a);
    for (size_t i = 0; i < NA; i++) a[i] = (int)i;

    sum_discarded(a, NA); sink += sum_kept(a, NA);      /* 데우기 */

    t0 = ns(); for (int r = 0; r < 20; r++) sum_discarded(a, NA); t1 = ns();
    double disc = (t1 - t0) / 20.0;
    t0 = ns(); for (int r = 0; r < 20; r++) sink += sum_kept(a, NA); t1 = ns();
    double kept = (t1 - t0) / 20.0;

    printf("  a sum whose result is discarded : %10.0f ns  (%.2f ns per element)\n", disc, disc / NA);
    printf("  a sum whose result is used      : %10.0f ns  (%.2f ns per element)\n", kept, kept / NA);
    printf("  factor                          : %.1f x\n", kept / (disc > 0 ? disc : 1));
    printf("  * if the first is near zero, that code never ran. The compiler removed it as\n");
    printf("    a value nobody uses. When the result says your code is infinitely fast,\n");
    printf("    suspect this before suspecting the machine.\n\n");

    printf("== 5. trap two: the first round is slow (warming up) ==\n");
    int *b = malloc(NA * sizeof *b);
    memset(b, 0, NA * sizeof *b);       /* 쪽을 실제로 잡아 둔다 */
    free(b);
    b = malloc(NA * sizeof *b);         /* 새로 잡으면 쪽이 아직 없다 */
    /* ★ 여기서도 ④ 의 함정이 먼저 걸렸다: 채운 결과를 아무도 안 읽으면 memset 이
       통째로 사라져 「4 MiB 를 38 나노초에 채웠다」는 헛것이 나온다. 그래서 잰 *뒤에*
       한 바이트를 읽어 결과가 쓰였음을 알린다. */
    t0 = ns(); memset(b, 1, NA * sizeof *b); t1 = ns();
    double first = t1 - t0;  sink += ((unsigned char *)b)[NA / 2];
    t0 = ns(); memset(b, 2, NA * sizeof *b); t1 = ns();
    double second = t1 - t0; sink += ((unsigned char *)b)[NA / 2];
    printf("  filling the same 4 MiB --- first  : %8.0f ns\n", first);
    printf("                          second : %8.0f ns\n", second);
    printf("  factor: %.1f x\n", first / second);
    printf("  * the first round mixes in the cost of the OS attaching pages one by one (page faults).\n");
    printf("    So warm up before measuring, and discard the warm-up rounds.\n\n");

    printf("== 6. trap three: one interruption drags the mean ==\n");
    const int S = 201;
    double *samp = malloc((size_t)S * sizeof *samp);
    for (int i = 0; i < S; i++) {
        t0 = ns(); sink += sum_kept(a, NA >> 4); t1 = ns();
        samp[i] = t1 - t0;
    }
    double mean = 0; for (int i = 0; i < S; i++) mean += samp[i]; mean /= S;
    qsort(samp, (size_t)S, sizeof *samp, cmp_d);
    printf("  results over %d rounds\n", S);
    printf("  %-10s %12.0f ns\n", "minimum", samp[0]);
    printf("  %-10s %12.0f ns  <- what this appendix uses\n", "median", samp[S / 2]);
    printf("  %-10s %12.0f ns\n", "mean", mean);
    printf("  %-10s %12.0f ns\n", "99th percentile", samp[(int)(S * 0.99)]);
    printf("  %-10s %12.0f ns\n", "maximum", samp[S - 1]);
    printf("  mean / median = %.2f x  (the further from 1, the more other work interfered)\n",
           mean / samp[S / 2]);
    printf("  * this machine is shared. So the median is used rather than the mean, with the\n");
    printf("    minimum alongside to show what it would be with no interference.\n");

    free(samp); free(b); free(a);
    return 0;
}
