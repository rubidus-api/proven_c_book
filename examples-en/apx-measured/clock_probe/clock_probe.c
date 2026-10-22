/* Before measuring, measure *the measuring tool*.
   Clock resolution, the cost of reading the clock, optimization deleting code, warm-up, and the trap of the mean. */
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

/* Work optimization may delete: nobody uses the result */
static void sum_discarded(const int *a, size_t n)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) s += a[i];
    (void)s;
}
/* Work optimization cannot delete: the result is returned and the caller uses it */
static long sum_kept(const int *a, size_t n)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) s += a[i];
    return s;
}
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

/* ★ The C library fills in cache sizes by asking the CPU. glibc on x86-64 answers with CPUID,
     but glibc on aarch64 (checked in the 2.44 source) answers only the line size, from CTR_EL0, and *0*
     for size and associativity --- the kernel keeps the register holding them from user programs.
     So on 0 we look at what the kernel read at boot and wrote to sysfs. Failing that it stays 0
     and the printing side says "unknown" --- it is never read as a 0-byte cache. */
static long sysfs_cache(int level, const char *field)
{
    const char *dir = "/sys/devices/system/cpu/cpu0/cache";
    for (int i = 0; i < 16; i++) {
        char path[128], buf[32];
        snprintf(path, sizeof path, "%s/index%d/level", dir, i);
        FILE *f = fopen(path, "r");
        if (!f) break;
        int lv = fgets(buf, sizeof buf, f) ? atoi(buf) : 0;
        fclose(f);
        snprintf(path, sizeof path, "%s/index%d/type", dir, i);
        f = fopen(path, "r");
        bool code_only = f && fgets(buf, sizeof buf, f) && strncmp(buf, "Instruction", 11) == 0;
        if (f) fclose(f);
        if (lv != level || code_only) continue;        /* skip instruction-only caches */
        snprintf(path, sizeof path, "%s/index%d/%s", dir, i, field);
        f = fopen(path, "r");
        if (!f) return 0;
        long v = 0;
        if (fgets(buf, sizeof buf, f)) {
            char *end;
            v = strtol(buf, &end, 10);
            if (*end == 'K') v *= 1024;                /* written in the form "32K" */
            else if (*end == 'M') v *= 1024 * 1024;
        }
        fclose(f);
        return v;
    }
    return 0;
}

static long cache_value(int name, int level, const char *field)
{
    long v = sysconf(name);
    return v > 0 ? v : sysfs_cache(level, field);
}

int main(void)
{
    printf("== 1. this machine's numbers (the example reads them itself) ==\n");
    const char *unknown = "unknown --- this system does not report it";
    long l1 = cache_value(_SC_LEVEL1_DCACHE_SIZE, 1, "size");
    long line = cache_value(_SC_LEVEL1_DCACHE_LINESIZE, 1, "coherency_line_size");
#if defined(__aarch64__) && defined(__linux__)
    if (line <= 0)
        line = 4L << ((read_ctr_el0() >> 16) & 0xf);
#endif
    long ways = cache_value(_SC_LEVEL1_DCACHE_ASSOC, 1, "ways_of_associativity");
    long l2 = cache_value(_SC_LEVEL2_CACHE_SIZE, 2, "size");
    long l3 = cache_value(_SC_LEVEL3_CACHE_SIZE, 3, "size");
    printf("  %-22s %s\n", "L1 data cache", "");
    if (l1 > 0)   printf("    size        : %ld bytes (%ld KiB)\n", l1, l1 / 1024);
    else          printf("    size        : %s\n", unknown);
    if (line > 0) printf("    line size   : %ld bytes\n", line);
    else          printf("    line size   : %s\n", unknown);
    if (ways > 0) printf("    associativity: %ld-way\n", ways);
    else          printf("    associativity: %s\n", unknown);
    if (l2 > 0)   printf("  L2 cache      : %ld KiB\n", l2 / 1024);
    else          printf("  L2 cache      : %s\n", unknown);
    if (l3 > 0)   printf("  L3 cache      : %ld KiB (%.0f MiB)\n", l3 / 1024, l3 / 1048576.0);
    else          printf("  L3 cache      : %s\n", unknown);
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

    /* the shortest interval we can actually see --- the smallest nonzero difference of two reads */
    double min_gap = 1e18;
    for (long i = 0; i < 100000; i++) {
        double a = ns(), b = ns();
        if (b - a > 0 && b - a < min_gap) min_gap = b - a;
    }
    printf("  smallest interval actually distinguishable : %.1f ns\n", min_gap);
    printf("  * so anything shorter than this is never measured once. Repeat and divide.\n");
    printf("    Every measurement in this appendix is built that way.\n\n");

    printf("== 4. trap one: the optimiser removes what you meant to measure ==\n");
    const size_t NA = 1u << 20;                 /* a 4 MiB array */
    int *a = malloc(NA * sizeof *a);
    for (size_t i = 0; i < NA; i++) a[i] = (int)i;

    sum_discarded(a, NA); sink += sum_kept(a, NA);      /* warm-up */

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
    memset(b, 0, NA * sizeof *b);       /* really claim the pages */
    free(b);
    b = malloc(NA * sizeof *b);         /* freshly allocated, the pages are not there yet */
    /* ★ Here too ④'s trap struck first: if nobody reads what was filled, the memset
       vanishes whole and you get the phantom "filled 4 MiB in 38 ns". So *after* measuring
       we read one byte to show the result was used. */
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
