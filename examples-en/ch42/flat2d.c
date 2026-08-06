/* Allocating in one dimension and using it as two — the commonest shape in practice.
   The subscript arithmetic is shut inside one macro instead of written by hand. */
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The subscript macro: every argument wrapped in parentheses (chapter 52), and the
   product raised to size_t. i and j are evaluated once, so side effects are safe. */
#define AT(p, cols, i, j)  ((p)[(size_t)(i) * (size_t)(cols) + (size_t)(j)])

/* The size computation can overflow — if it does, no allocation is attempted (ch. 49, 70). */
static int *alloc_grid(size_t rows, size_t cols)
{
    size_t cells, bytes;
    if (ckd_mul(&cells, rows, cols))          return NULL;
    if (ckd_mul(&bytes, cells, sizeof(int)))  return NULL;
    return malloc(bytes);
}

int main(void)
{
    const size_t rows = 3, cols = 4;
    int *g = alloc_grid(rows, cols);
    if (!g) { puts("allocation failed"); return 1; }

    /* ── (1) filling and reading — one macro does the subscripts ─── */
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
            AT(g, cols, i, j) = (int)(i * 10 + j);

    printf("a 3x4 held in row-major order:\n");
    for (size_t i = 0; i < rows; i++) {
        printf("  ");
        for (size_t j = 0; j < cols; j++)
            printf("%3d", AT(g, cols, i, j));
        printf("\n");
    }

    /* ── (2) the memory is one continuous run ─────────────────────── */
    printf("\nsweeping the same memory in one dimension: ");
    for (size_t k = 0; k < rows * cols; k++)
        printf(k ? " %d" : "%d", g[k]);
    printf("\n(the same layout as a real int m[3][4] — row-major)\n");

    /* ── (3) the common accident: row and column swapped ──────────── */
    printf("\nAT(g, cols, 1, 2) = %d   (row 2, item 3)\n", AT(g, cols, 1, 2));
    printf("AT(g, cols, 2, 1) = %d   (row 3, item 2 — a different slot)\n", AT(g, cols, 2, 1));

    /* ── (4) an overflowing size request is refused ───────────────── */
    int *huge = alloc_grid((size_t)-1 / 2, 8);
    printf("\nan absurd size request: %s\n", huge ? "allocated" : "refused, the computation overflowed");
    free(huge);

    free(g);
    return 0;
}
