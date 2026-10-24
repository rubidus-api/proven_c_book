/* The alignment of the address malloc returns, and the price of borrowing and returning */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 300000

static double seconds_since(struct timespec t0)
{
    struct timespec t1;
    timespec_get(&t1, TIME_UTC);
    return (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
}

static void *volatile sink;      /* holds the allocation so optimisation cannot erase it */

int main(void)
{
    /* ── (1) alignment: not knowing what will be put in it, it gives the strictest ── */
    printf("alignment of max_align_t = %zu bytes\n", alignof(max_align_t));

    size_t misaligned = 0;
    void *p[8];
    for (int i = 0; i < 8; i++) {
        p[i] = malloc(1);                    /* even asking for a single byte */
        if ((uintptr_t)p[i] % alignof(max_align_t) != 0) misaligned++;
    }
    printf("of 8 one-byte requests, ones breaking the default alignment: %zu\n", misaligned);
    printf("address gap between two neighbouring blocks: %td bytes (for a request of 1)\n",
           (char *)p[1] - (char *)p[0]);
    for (int i = 0; i < 8; i++) free(p[i]);

    /* ── (2) the price: the same work three ways ── */
    struct timespec t0;
    unsigned long long acc = 0;

    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < N; i++) {           /* borrow and return every time */
        char *b = malloc(64);
        if (!b) return 1;
        b[0] = (char)i;
        acc += (unsigned char)b[0];
        sink = b;
        free(b);
    }
    double t_alloc = seconds_since(t0);

    char *reused = malloc(64);
    if (!reused) return 1;
    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < N; i++) {           /* borrow once and keep using it */
        reused[0] = (char)i;
        acc += (unsigned char)reused[0];
        sink = reused;
    }
    double t_reuse = seconds_since(t0);
    free(reused);

    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < N; i++) {           /* put it on the stack instead */
        char b[64];
        b[0] = (char)i;
        acc += (unsigned char)b[0];
        sink = b;
    }
    double t_stack = seconds_since(t0);

    printf("\n%d iterations\n", N);
    printf("  malloc + free each time : %7.4f s  (%5.1f ns each)\n", t_alloc, t_alloc / N * 1e9);
    printf("  borrowed once, reused   : %7.4f s  (%5.1f ns each)\n", t_reuse, t_reuse / N * 1e9);
    printf("  a stack array           : %7.4f s  (%5.1f ns each)\n", t_stack, t_stack / N * 1e9);
    printf("\n(total %llu — a value used so the computation is not erased)\n", acc);
    return 0;
}
