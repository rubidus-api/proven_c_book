/* 주소 번역에도 값이 있다 --- TLB 를 넘어서면 무슨 일이 벌어지나.
   쪽마다 한 바이트씩만 무작위로 밟아, 캐시가 아니라 *번역*이 병목이 되게 만든다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

static double ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static uint64_t st = 0x2545F4914F6CDD1Dull;
static uint64_t rnd(void) { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; }


/* 커널이 「큰 쪽」 권고를 실제로 받아들였는가 --- /proc/self/smaps 에서 이 구간을 찾아
   AnonHugePages 값을 읽는다. 권고는 부탁이지 명령이 아니므로 *확인해야 한다.* */
static long anon_huge_kib(const void *addr)
{
    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) return -1;
    char line[512];
    unsigned long lo = 0, hi = 0;
    int in_range = 0;
    long result = -1;
    while (fgets(line, sizeof line, f)) {
        unsigned long a, b;
        if (sscanf(line, "%lx-%lx", &a, &b) == 2 && strchr(line, ' ')) {
            lo = a; hi = b;
            in_range = ((uintptr_t)addr >= lo && (uintptr_t)addr < hi);
        } else if (in_range && strncmp(line, "AnonHugePages:", 14) == 0) {
            result = strtol(line + 14, NULL, 10);
            break;
        }
    }
    fclose(f);
    return result;
}

static volatile size_t sink;

/* 쪽 하나에 한 칸씩 --- 쪽 사이를 무작위 고리로 잇는다 */
static double walk(unsigned char *mem, size_t pages, size_t page, long steps)
{
    size_t *order = malloc(pages * sizeof *order);
    for (size_t i = 0; i < pages; i++) order[i] = i;
    for (size_t i = pages - 1; i > 0; i--) {
        size_t j = (size_t)(rnd() % i);
        size_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    /* 각 쪽의 첫 8바이트에 「다음 쪽의 오프셋」을 적어 고리를 만든다 */
    for (size_t i = 0; i < pages; i++) {
        size_t cur = order[i], nxt = order[(i + 1) % pages];
        *(size_t *)(mem + cur * page) = nxt * page;
    }
    free(order);

    size_t p = 0;
    for (size_t i = 0; i < pages; i++) p = *(size_t *)(mem + p);      /* 데우기 */
    double t0 = ns();
    for (long i = 0; i < steps; i++) p = *(size_t *)(mem + p);
    double t1 = ns();
    sink = p;
    return (t1 - t0) / (double)steps;
}

int main(void)
{
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    printf("== the page size of this machine: %zu bytes ==\n\n", page);

    printf("== 1. how many pages before translation becomes the bottleneck ==\n");
    printf("  only 8 bytes are touched per page. Little data, but the page count grows.\n\n");
    printf("  %-10s %-12s %-12s %s\n", "pages", "data touched", "each", "factor");
    printf("#DATA-BEGIN\n");
    double base = 0;
    for (size_t pages = 16; pages <= 131072; pages *= 4) {
        size_t bytes = pages * page;
        unsigned char *mem = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) { printf("  (%zu pages: allocation failed)\n", pages); break; }
        memset(mem, 0, bytes);                       /* 쪽을 미리 붙인다 */

        double s[5];
        for (int r = 0; r < 5; r++) s[r] = walk(mem, pages, page, 2000000);
        qsort(s, 5, sizeof *s, cmp_d);
        if (base == 0) base = s[2];

        char amount[24];
        if (bytes < (1u << 20)) snprintf(amount, sizeof amount, "%zu KiB", bytes / 1024);
        else                    snprintf(amount, sizeof amount, "%zu MiB", bytes / (1u << 20));
        printf("  %-10zu %-12s %8.2f ns %6.1fx\n", pages, amount, s[2], s[2] / base);
        printf("#DATA %zu %.3f\n", pages, s[2]);
        munmap(mem, bytes);
    }
    printf("#DATA-END\n");
    printf("\n  * the data is small (8 bytes per page), and yet more pages costs more.\n");
    printf("    The bottleneck is address translation, not the cache. When the table\n");
    printf("    holding translations (the TLB) runs out of room, the machine must walk\n    the page tables again.\n");

    printf("\n== 2. what happens with huge pages ==\n");
    const size_t big_pages = 65536;                 /* 4 KiB × 65536 = 256 MiB */
    const size_t bytes = big_pages * page;
    double normal = 0, huge = 0;
    for (int mode = 0; mode < 2; mode++) {
        unsigned char *mem = mmap(NULL, bytes + (2u << 20), PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) break;
        unsigned char *aligned = (unsigned char *)(((uintptr_t)mem + (2u << 20) - 1) & ~(uintptr_t)((2u << 20) - 1));
#ifdef MADV_HUGEPAGE
        madvise(aligned, bytes, mode ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
#endif
        memset(aligned, 0, bytes);
        long ah = anon_huge_kib(aligned);
        double s[5];
        for (int r = 0; r < 5; r++) s[r] = walk(aligned, big_pages, page, 2000000);
        qsort(s, 5, sizeof *s, cmp_d);
        if (mode) huge = s[2]; else normal = s[2];
        printf("  %-28s backed by huge pages: %ld KiB (%.0f%%)\n",
               mode ? "huge pages advised" : "normal pages advised", ah,
               ah < 0 ? 0.0 : 100.0 * (double)ah * 1024.0 / (double)bytes);
        munmap(mem, bytes + (2u << 20));
    }
    printf("  the same 256 MiB is walked the same way.\n");
    printf("  %-28s %8.2f ns\n", "normal pages (4 KiB)", normal);
    printf("  %-28s %8.2f ns\n", "huge pages advised (MADV_HUGEPAGE)", huge);
    if (huge > 0 && normal > 0)
        printf("  difference: %.2f x %s\n", normal / huge,
               normal / huge > 1.15 ? "--- huge pages won" : "--- no great difference in this run");
    printf("  * one huge page replaces 512 pages of 4 KiB, so the translations needed to\n");
    printf("    sweep the same data fall to a 512th. But madvise is a request, not an order.\n");
    printf("    The \"backed by huge pages\" figure above says whether the request was taken ---\n");
    printf("    if both runs are backed alike, similar times are only to be expected.\n");
    printf("    (this machine uses huge pages by default, so even the \"normal pages\" run\n");
    printf("     may already have some mixed in.)\n");

    printf("\n== 3. the cost of touching a page for the first time ==\n");
    const size_t fp = 32768;                        /* 128 MiB */
    unsigned char *mem = mmap(NULL, fp * page, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    double t0 = ns();
    for (size_t i = 0; i < fp; i++) mem[i * page] = 1;      /* 쪽마다 첫 접촉 */
    double t1 = ns();
    double first = (t1 - t0) / (double)fp;
    t0 = ns();
    for (size_t i = 0; i < fp; i++) mem[i * page] = 2;      /* 이미 붙은 쪽 */
    t1 = ns();
    double later = (t1 - t0) / (double)fp;
    munmap(mem, fp * page);
    printf("  touching a page the first time : %8.1f ns\n", first);
    printf("  touching an already-mapped page: %8.1f ns\n", later);
    printf("  factor: %.0f x\n", first / later);
    printf("  * the address `malloc` returned is not memory yet. At each first touch the\n");
    printf("    operating system attaches one page --- and that is its cost. Allocate a large\n");
    printf("    buffer and measure at once and this cost is mixed in entire (hence warming up).\n");
    return 0;
}
