/* 갈림길에는 값이 있다 --- 그런데 그 값은 「분기가 있느냐」가 아니라
   「그 분기를 기계가 맞힐 수 있느냐」가 정한다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static double ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static int cmp_i(const void *a, const void *b)
{ int x = *(const int *)a, y = *(const int *)b; return (x > y) - (x < y); }

static uint64_t st = 0x9E3779B97F4A7C15ull;
static uint64_t rnd(void) { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; }

static volatile long sink;

#define N (1u << 22)      /* 원소 400만 개 = 16 MiB */
#define R 5               /* 회차 */

/* 조건에 걸리면 더한다 --- 분기가 있는 코드 */
static long sum_branch(const int *a, size_t n, int t)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) if (a[i] >= t) s += a[i];
    return s;
}
/* ★ 위 함수를 -O2 로 컴파일하면 컴파일러가 `if` 를 *조건 이동*(cmov)으로 바꿔 버린다.
   그러면 예측할 분기가 아예 없어 「정렬하면 빨라진다」는 고전적 현상이 사라진다.
   그래서 *진짜 분기를 남긴 판*을 따로 둔다 --- 조건 이동과 벡터화를 이 함수에서만 끈다. */
__attribute__((optimize("no-if-conversion", "no-if-conversion2", "no-tree-vectorize")))
static long sum_realbranch(const int *a, size_t n, int t)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) if (a[i] >= t) s += a[i];
    return s;
}

/* 같은 계산, 분기 없이 --- 조건을 *산술*로 바꾼다 */
static long sum_branchless(const int *a, size_t n, int t)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) {
        long mask = -(long)(a[i] >= t);      /* 참이면 -1(전부 1), 거짓이면 0 */
        s += a[i] & mask;
    }
    return s;
}

static double time_it(long (*fn)(const int *, size_t, int), const int *a, int t)
{
    double s[R];
    for (int r = 0; r < R; r++) {
        double t0 = ns();
        sink = fn(a, N, t);
        double t1 = ns();
        s[r] = (t1 - t0) / (double)N;
    }
    qsort(s, R, sizeof *s, cmp_d);
    return s[R / 2];
}

int main(void)
{
    int *a = malloc(N * sizeof *a);
    for (size_t i = 0; i < N; i++) a[i] = (int)(rnd() % 256);
    int *sorted = malloc(N * sizeof *sorted);
    memcpy(sorted, a, N * sizeof *a);
    qsort(sorted, N, sizeof *sorted, cmp_i);

    printf("== the same data, the same computation, only the order differs ==\n");
    printf("  summing only the values of 128 or more, over %u elements (16 MiB).\n", N);
    printf("  the two arrays hold exactly the same contents --- only sortedness differs.\n\n");

    double rand_src = time_it(sum_branch, a, 128);
    double sort_src = time_it(sum_branch, sorted, 128);
    double rand_br  = time_it(sum_realbranch, a, 128);
    double sort_br  = time_it(sum_realbranch, sorted, 128);
    double rand_bl  = time_it(sum_branchless, a, 128);
    double sort_bl  = time_it(sum_branchless, sorted, 128);

    printf("  %-34s %12s %12s %s\n", "", "random order", "sorted order", "sorted/random");
    printf("  %-34s %9.3f ns %9.3f ns %7.2f x\n",
           "source with an if (plain -O2)", rand_src, sort_src, rand_src / sort_src);
    printf("  %-34s %9.3f ns %9.3f ns %7.2f x\n",
           "code that kept a real branch", rand_br, sort_br, rand_br / sort_br);
    printf("  %-34s %9.3f ns %9.3f ns %7.2f x\n",
           "code with the branch turned into arithmetic", rand_bl, sort_bl, rand_bl / sort_bl);

    printf("\n  * the first two rows have the same source. Only the compiler settings differ.\n");
    printf("    In the first, the compiler turned the `if` into a conditional move (cmov), so\n");
    printf("    there is no branch to predict and sorting brings no gain. That is why the\n");
    printf("    famous \"sorted arrays are faster\" story often fails to reproduce today.\n\n");

    /* 틀린 예측 한 번의 값 --- 무작위면 절반쯤 틀린다고 보고 나눈다 */
    double extra_per_elem = rand_br - sort_br;
    printf("  extra time per element in the real-branch version: %.3f ns\n", extra_per_elem);
    printf("  / a miss probability of 0.5 = about %.1f ns per misprediction\n", extra_per_elem / 0.5);

    printf("\n== how regular must it be to be predicted ==\n");
    printf("  the same number of trues, with only the pattern changed.\n\n");
    printf("  %-28s %12s %s\n", "pattern", "each", "note");
    struct { const char *name; int period; } pats[] = {
        { "always true",             1 },
        { "alternating",        2 },
        { "one in four",       4 },
        { "one in sixteen",  16 },
        { "random",                0 },
    };
    printf("#DATA-BEGIN\n");
    for (unsigned p = 0; p < sizeof pats / sizeof *pats; p++) {
        for (size_t i = 0; i < N; i++) {
            int take = pats[p].period == 0 ? (int)(rnd() & 1)
                     : (int)(i % (size_t)pats[p].period == 0);
            a[i] = take ? 200 : 10;
        }
        double v = time_it(sum_realbranch, a, 128);
        printf("  %-28s %9.3f ns %s\n", pats[p].name, v,
               pats[p].period == 0 ? "cannot be predicted"
               : pats[p].period == 1 ? "always predicted" : "short periods are memorised");
        printf("#DATA %d %.3f\n", pats[p].period, v);
    }
    printf("#DATA-END\n");

    printf("\n== how to read this ==\n");
    printf("  0. today's compilers turn a short `if` into branchless code. So to measure\n");
    printf("     branch prediction you must first check that a branch survives.\n");
    printf("  1. \"branches are expensive\" is imprecise. On a sorted array a branch is cheap.\n");
    printf("     What is expensive is a branch that cannot be predicted.\n");
    printf("  2. on a random pattern the machine is wrong about half the time, and each miss\n");
    printf("     empties and refills the pipeline --- the tens of nanoseconds computed above.\n");
    printf("  3. short-period patterns are memorised, so even \"one in four\" is fast.\n");
    printf("  4. branchless code takes the same time in any order --- but it always computes\n");
    printf("     every element, so for an easily predicted branch it is a loss.\n");

    free(a); free(sorted);
    return 0;
}
