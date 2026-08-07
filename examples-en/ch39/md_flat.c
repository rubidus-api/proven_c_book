/* Allocate flat, view as 2-D — the BLAS-style pattern (leading dimension, submatrix). */
#include <stdio.h>
#include <stdlib.h>

/* Leading dimension (lda): the *stride* from one row to the next.
   It may differ from the column count — which is what makes submatrices free. */
#define AT(a, lda, i, j) ((a)[(size_t)(i) * (size_t)(lda) + (size_t)(j)])

static void fill(double *a, size_t lda, size_t rows, size_t cols)
{
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
            AT(a, lda, i, j) = (double)(10 * (i + 1) + (j + 1));
}

static void show(const char *tag, const double *a, size_t lda,
                 size_t rows, size_t cols)
{
    printf("%s (lda=%zu):\n", tag, lda);
    for (size_t i = 0; i < rows; i++) {
        printf("  ");
        for (size_t j = 0; j < cols; j++) printf("%5.0f", AT(a, lda, i, j));
        printf("\n");
    }
}

int main(void)
{
    const size_t rows = 4, cols = 5;

    /* one run — check the multiplication for overflow first (chapter 85's habit) */
    size_t n;
    if (__builtin_mul_overflow(rows, cols, &n)) return 1;
    double *m = malloc(n * sizeof *m);
    if (!m) return 1;

    fill(m, cols, rows, cols);
    show("whole 4x5", m, cols, rows, cols);

    /* (1) A 2-D view — memory from malloc has no declared type, so putting
          an array shape on it is the sound direction (see the text). */
    double (*view)[cols] = (double (*)[cols])m;
    printf("\nthrough the view, view[2][3] = %.0f  (flat: m[2*5+3] = %.0f)\n",
           view[2][3], m[2 * 5 + 3]);

    /* (2) A submatrix — pointing, not copying.
          Rows 1..2, columns 1..3: a 2x3 block. The stride stays 5. */
    double *sub = &AT(m, cols, 1, 1);
    printf("\na submatrix is a start plus a stride, not a copy:\n");
    show("  sub 2x3", sub, cols, 2, 3);

    /* write through the submatrix and the original changes — it is a view */
    AT(sub, cols, 0, 0) = -1;
    printf("\nwriting -1 at sub(0,0) changes the original at (1,1):\n");
    show("whole 4x5", m, cols, rows, cols);

    /* (3) Transposition becomes a stride question too — only the reading order changes */
    printf("\nreading transposed (row step 1, column step %zu):\n", cols);
    for (size_t j = 0; j < cols; j++) {
        printf("  ");
        for (size_t i = 0; i < rows; i++) printf("%5.0f", AT(m, cols, i, j));
        printf("\n");
    }

    free(m);
    return 0;
}
