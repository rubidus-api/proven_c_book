/* Measure the memory ladder --- grow the working set and watch "the time for one read".
   The trick is *pointer chasing*: when the next place to read is in the value just read, the
   machine cannot prefetch and goes one at a time. So *latency*, not throughput, shows. */
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

/* Sattolo's algorithm: make *one ring* through the whole array (it never splits into small rings) */
static void make_cycle(size_t *next, size_t n)
{
    for (size_t i = 0; i < n; i++) next[i] = i;
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(rnd() % i);
        size_t t = next[i]; next[i] = next[j]; next[j] = t;
    }
    /* turn the permutation into a ring: link next[a]=b so it reads "after a comes b" */
    size_t *cyc = malloc(n * sizeof *cyc);
    for (size_t i = 0; i < n; i++) cyc[next[i]] = next[(i + 1) % n];
    memcpy(next, cyc, n * sizeof *cyc);
    free(cyc);
}

static volatile size_t sink;

/* Step along the random ring `steps` times --- every step depends on the one before */
static double chase(size_t *next, size_t n, long steps)
{
    size_t p = 0;
    for (long i = 0; i < (long)n * 4 && i < 4000000L; i++) p = next[p];   /* warm-up */
    double t0 = ns();
    for (long i = 0; i < steps; i++) p = next[p];
    double t1 = ns();
    sink = p;
    return (t1 - t0) / (double)steps;
}

/* For comparison: scan in order --- the machine can prefetch the next place */
static double sweep(size_t *buf, size_t n, long rounds)
{
    size_t acc = 0;
    for (size_t i = 0; i < n; i++) acc += buf[i];                    /* warm-up */
    double t0 = ns();
    for (long r = 0; r < rounds; r++)
        for (size_t i = 0; i < n; i++) acc += buf[i];
    double t1 = ns();
    sink = acc;
    return (t1 - t0) / ((double)n * (double)rounds);
}

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
    const long L1 = cache_value(_SC_LEVEL1_DCACHE_SIZE, 1, "size");
    const long L2 = cache_value(_SC_LEVEL2_CACHE_SIZE, 2, "size");
    const long L3 = cache_value(_SC_LEVEL3_CACHE_SIZE, 3, "size");
    const bool known = L1 > 0 || L2 > 0 || L3 > 0;

    printf("== the caches of this machine ==\n");
    if (known)
        printf("  L1 %ld KiB · L2 %ld KiB · L3 %ld KiB (%.0f MiB)\n\n",
               L1 / 1024, L2 / 1024, L3 / 1024, L3 / 1048576.0);
    else
        printf("  unknown --- this system does not report them, so the \"where\" column shows ?\n\n");

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
        double lat = s[2];                       /* the median */

        /* scanning in order: same size, in sequence --- throughput shows, not latency */
        for (size_t i = 0; i < n; i++) buf[i] = i;
        long rounds = bytes < (1u << 22) ? 200 : 5;
        double seq = sweep(buf, n, rounds);

        /* skip unknown levels --- compared with size 0 everything would read as "main memory" */
        const char *where = !known                          ? "?"
                          : L1 > 0 && (long)bytes <= L1     ? "L1"
                          : L2 > 0 && (long)bytes <= L2     ? "L2"
                          : L3 > 0 && (long)bytes <= L3     ? "L3" : "main memory";
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
