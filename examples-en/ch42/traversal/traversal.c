/* Same sum, different order --- ask the machine what traversal order changes.
   ★ This example measures time. The numbers differ by machine, by run, and by *optimization
     level*. It keeps the same discipline as order.c in chapter 12 --- no absolute times,
     only ratios. An absolute time means nothing unless you say whose machine
     it came from.
   ★ Both loops are made to do exactly the same work. If all values were equal the compiler
     would fold the sum into a constant and delete the loop --- measured that way at first, we got
     the impossible "row-major 0.0 ms". So the values are scattered and the result is consumed. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N      4000          /* 4000×4000 int = 64 MB --- far larger than the cache */
#define ROUNDS 5

static int (*a)[N];
static volatile long long sink;

static double elapsed_ms(struct timespec s, struct timespec e)
{
    return (double)(e.tv_sec - s.tv_sec) * 1e3
         + (double)(e.tv_nsec - s.tv_nsec) / 1e6;
}

int main(void)
{
    a = malloc(sizeof *a * N);
    if (!a) { puts("out of memory"); return 1; }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            a[i][j] = (i * 31 + j) & 0xff;   /* scatter the values */

    struct timespec s, e;
    double best_row = 1e18, best_col = 1e18;

    for (int r = 0; r < ROUNDS; r++) {
        long long sum = 0;
        timespec_get(&s, TIME_UTC);
        for (int i = 0; i < N; i++)          /* row-major --- along the lines */
            for (int j = 0; j < N; j++)
                sum += a[i][j];
        timespec_get(&e, TIME_UTC);
        sink = sum;
        if (elapsed_ms(s, e) < best_row) best_row = elapsed_ms(s, e);

        sum = 0;
        timespec_get(&s, TIME_UTC);
        for (int j = 0; j < N; j++)          /* column-major --- across the lines */
            for (int i = 0; i < N; i++)
                sum += a[i][j];
        timespec_get(&e, TIME_UTC);
        sink = sum;
        if (elapsed_ms(s, e) < best_col) best_col = elapsed_ms(s, e);
    }

    printf("  summing a %d x %d int array, %d rounds, best of each\n",
           N, N, ROUNDS);
    printf("  both loops add the same %lld values; only the order differs.\n",
           (long long)N * N);
    printf("  column-major took %.1f times as long as row-major\n",
           best_col / best_row);
    free(a);
    return 0;
}
