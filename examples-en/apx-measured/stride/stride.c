/* Does the cache "line" show up in time? --- measure while changing the stride.
   Then measure the same data in a different layout (array of structs vs struct of arrays). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

static volatile long sink;

/* ★ aarch64 Linux lets user programs read the CTR_EL0 register too (glibc answers the line
     size from it). Android's Bionic returns 0 from sysconf, and phone kernels may leave the sysfs
     size empty (measured on an Android phone), so as a last resort we read this register directly.
     It gives the *smallest* line size in the whole hierarchy, though. */
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
    return 0;                                  /* unknown --- the caller says so */
#endif
}

int main(void)
{
    long line = cache_line();
    if (line <= 0) {                         /* if unknown, use the common value but say it is an assumption */
        line = 64;
        printf("(this system does not report its cache line --- 64 bytes is assumed below)\n");
    }
    const size_t BUF = 64u << 20;            /* 64 MiB --- larger than L3 (16 MiB) */
    const long TOUCH = 2000000;              /* *the same number of accesses* whatever the stride */
    unsigned char *buf = malloc(BUF);
    memset(buf, 1, BUF);                     /* fault the pages in first (warm-up) */

    printf("== the cache line of this machine: %ld bytes ==\n\n", line);
    printf("== varying the stride --- the number of accesses fixed at %ld ==\n", TOUCH);
    printf("  %-10s %-14s %-10s %-16s %s\n",
           "stride", "each", "factor", "lines touched", "bytes fetched in vain");
    printf("#DATA-BEGIN\n");

    double base = 0;
    for (long stride = 1; stride <= 4096; stride *= 2) {
        size_t mask = BUF - 1;               /* BUF is a power of two, so & can wrap it */
        double s[5];
        for (int r = 0; r < 5; r++) {
            size_t p = 0;
            long acc = 0;
            double t0 = ns();
            for (long i = 0; i < TOUCH; i++) { acc += buf[p]; p = (p + (size_t)stride) & mask; }
            double t1 = ns();
            sink = acc;
            s[r] = (t1 - t0) / (double)TOUCH;
        }
        qsort(s, 5, sizeof *s, cmp_d);
        if (base == 0) base = s[2];
        double lines_per_touch = stride >= line ? 1.0 : (double)stride / (double)line;
        double wasted = stride >= line ? (double)line - 1 : 0;
        printf("  %-10ld %8.2f ns %7.1fx  %-16.3f %.0f bytes\n",
               stride, s[2], s[2] / base, lines_per_touch, wasted);
        printf("#DATA %ld %.3f\n", stride, s[2]);
    }
    printf("#DATA-END\n");

    printf("\n  * the cost rises until the stride reaches %ld bytes (the line size), and\n", line);
    printf("    then flattens. A stride narrower than a line uses one line several times,\n");
    printf("    while a wider one takes a new line each step and has little room to worsen.\n");
    printf("    (the rise again at very wide strides is pages and the TLB --- the next section.)\n");

    /* ── change the layout --- array of structs vs struct of arrays ──── */
    printf("\n== the same data, laid out differently ==\n");
    const size_t N = 4u << 20;               /* 4 million elements */

    struct particle { double x, y, z, vx, vy, vz; int id, flags; };  /* the real size is printed below */
    struct particle *aos = malloc(N * sizeof *aos);
    double *xs = malloc(N * sizeof *xs);
    for (size_t i = 0; i < N; i++) {
        aos[i].x = xs[i] = (double)i * 0.5;
        aos[i].y = aos[i].z = aos[i].vx = aos[i].vy = aos[i].vz = 1.0;
        aos[i].id = (int)i; aos[i].flags = 0;
    }
    printf("  size of one struct : %zu bytes (line %ld bytes)\n", sizeof(struct particle), line);
    printf("  %zu elements --- %zu MiB in total\n\n", N, N * sizeof *aos / (1u << 20));

    double s1[5], s2[5];
    for (int r = 0; r < 5; r++) {
        double acc = 0, t0 = ns();
        for (size_t i = 0; i < N; i++) acc += aos[i].x;      /* only x, from the array of structs */
        double t1 = ns(); sink = (long)acc; s1[r] = (t1 - t0) / (double)N;

        acc = 0; t0 = ns();
        for (size_t i = 0; i < N; i++) acc += xs[i];         /* from an array holding only x */
        t1 = ns(); sink = (long)acc; s2[r] = (t1 - t0) / (double)N;
    }
    qsort(s1, 5, sizeof *s1, cmp_d); qsort(s2, 5, sizeof *s2, cmp_d);

    printf("  %-34s %10s %10s %s\n", "layout", "per element", "factor", "bytes used per line");
    printf("  %-34s %7.3f ns %8.1fx  %ld / %ld\n",
           "array of structs (AoS) --- reading x only", s1[2], s1[2] / s2[2],
           (long)sizeof(double), (long)sizeof(struct particle));
    printf("  %-34s %7.3f ns %8.1fx  %ld / %ld\n",
           "structs of arrays (SoA) --- reading the x array only", s2[2], 1.0, line, line);

    printf("\n  * the same values were added the same number of times. Only the layout changed.\n");
    printf("    In the array of structs, %zu bytes are fetched to use 8 --- the rest merely\n",
           sizeof(struct particle));
    printf("    occupy cache and are thrown away. So when one field is swept often,\n");
    printf("    splitting into one array per field (SoA) wins.\n");
    printf("  * the reverse holds too. Code using several fields of one element together\n");
    printf("    prefers the array of structs --- there the whole fetched line is used. The access pattern decides.\n");

    free(aos); free(xs); free(buf);
    return 0;
}
