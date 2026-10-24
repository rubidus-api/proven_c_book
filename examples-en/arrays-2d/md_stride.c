/* How a traversal order sweeps memory — we count *access strides*, not time.
   (Timing differs per build, so only deterministic numbers are printed.) */
#include <stdio.h>
#include <stdlib.h>

#define LINE 64            /* the size of one cache line (chapter 11) */

/* Walk the order, take the cache-line number of each access, and count how
   many distinct lines appear and how often it is the same line as last time. */
typedef struct { size_t lines_touched; size_t same_line_hits; } stats;

static stats scan(const double *base, const size_t *order, size_t n)
{
    stats s = { 0, 0 };
    long prev_line = -1;
    unsigned char seen[4096] = {0};
    for (size_t k = 0; k < n; k++) {
        size_t byte = order[k] * sizeof(double);
        size_t line = byte / LINE;
        if (line < sizeof seen && !seen[line]) { seen[line] = 1; s.lines_touched++; }
        if ((long)line == prev_line) s.same_line_hits++;
        prev_line = (long)line;
    }
    (void)base;
    return s;
}

int main(void)
{
    enum { N = 64 };                       /* 64x64 doubles = 32 KiB */
    static double m[N][N];
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++) m[i][j] = (double)(i * N + j);

    size_t *order = malloc((size_t)N * N * sizeof *order);
    if (!order) return 1;

    /* (1) Row-major order: m[i][j] with j innermost */
    size_t k = 0;
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++) order[k++] = i * N + j;
    stats row = scan(&m[0][0], order, (size_t)N * N);

    /* (2) Column order (transposed sweep): m[i][j] with i innermost */
    k = 0;
    for (size_t j = 0; j < N; j++)
        for (size_t i = 0; i < N; i++) order[k++] = i * N + j;
    stats col = scan(&m[0][0], order, (size_t)N * N);

    printf("Reading a 64x64 double matrix 4096 times. Cache line %d bytes,\n", LINE);
    printf("so %zu doubles fit in one line.\n\n", (size_t)LINE / sizeof(double));

    printf("%-14s %14s %18s\n", "order", "lines touched", "same line as last");
    printf("%-14s %14zu %18zu\n", "row-major ij", row.lines_touched, row.same_line_hits);
    printf("%-14s %14zu %18zu\n", "column ji", col.lines_touched, col.same_line_hits);

    printf("\nBoth sweeps touch the same number of lines — they read the same data.\n");
    printf("What differs is *continuity*. Row-major settles %zu accesses in the\n",
           row.same_line_hits);
    printf("line already loaded; column order manages %zu. Its step is %zu bytes,\n",
           col.same_line_hits, (size_t)N * sizeof(double));
    printf("so it jumps to a different line every time.\n");

    free(order);
    return 0;
}
