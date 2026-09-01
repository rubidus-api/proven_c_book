/* 약속의 값은 나중에 치른다 --- 쪽을 처음 만지는 접촉과 두 번째 접촉을 견준다.

   첫 접촉에서 벌어지는 일: 번역이 없어 오류가 나고, 커널이 쪽 하나를 찾아
   0 으로 채우고, 쪽 표에 적고, 돌아온다. 두 번째 접촉은 그냥 쓰기다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>

static double ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

int main(void)
{
    const size_t PAGE = 4096, PAGES = 64u * 1024;      /* 256 MiB */
    const int R = 5;
    double first[5], again[5];

    printf("== what a page costs the first time it is touched ==\n\n");
    printf("#DATA-BEGIN\n");

    for (int r = 0; r < R; r++) {
        char *p = mmap(NULL, PAGE * PAGES, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            puts("mmap refused; this machine cannot run the experiment.");
            return 0;
        }
        double t0 = ns();
        for (size_t i = 0; i < PAGES; i++) p[i * PAGE] = 1;
        double t1 = ns();
        for (size_t i = 0; i < PAGES; i++) p[i * PAGE] = 2;
        double t2 = ns();
        first[r] = (t1 - t0) / (double)PAGES;
        again[r] = (t2 - t1) / (double)PAGES;
        munmap(p, PAGE * PAGES);
    }
    qsort(first, R, sizeof *first, cmp_d);
    qsort(again, R, sizeof *again, cmp_d);

    printf("  %-16s %10.1f ns per page\n", "first touch", first[R / 2]);
    printf("  %-16s %10.1f ns per page\n", "touched again", again[R / 2]);
    printf("  %-16s %10.1f x\n", "factor", first[R / 2] / again[R / 2]);
    printf("#DATA first %.1f\n", first[R / 2]);
    printf("#DATA again %.1f\n", again[R / 2]);
    printf("#DATA factor %.1f\n", first[R / 2] / again[R / 2]);
    printf("#DATA-END\n");

    puts("\n  * the second write is a write. The first one is a fault, a page, a zeroing");
    puts("    and an entry in the page table -- the bill for the memory that was");
    puts("    promised earlier and not delivered until now.");
    return 0;
}
