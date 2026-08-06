/* Three layouts for passing a 2-D array to a function — and why it is not int**. */
#include <stdio.h>
#include <stdlib.h>

/* (1) Fixed width: the inner dimension stays in the type.
      The three spellings are the same declaration to the compiler. */
static int sum_fixed(int m[3][4])      { int s = 0; for (int i=0;i<3;i++) for (int j=0;j<4;j++) s += m[i][j]; return s; }
/* These two are *exactly the same* declaration (see the text):
       static int sum_fixed(int m[][4]);
       static int sum_fixed(int (*m)[4]);   */

/* (2) VLA parameter (C99): the width arrives at run time — the way for numeric code */
static int sum_vla(size_t rows, size_t cols, const int a[rows][cols])
{
    int s = 0;
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++) s += a[i][j];
    return s;
}

/* (3) An array of row pointers: a different layout — rows may live apart */
static int sum_rows(size_t rows, size_t cols, int *const rowp[rows])
{
    int s = 0;
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++) s += rowp[i][j];
    return s;
}

int main(void)
{
    int a[3][4] = { {1,2,3,4}, {5,6,7,8}, {9,10,11,12} };

    printf("(1) fixed width  sum_fixed(a)      = %d\n", sum_fixed(a));
    printf("(2) VLA param    sum_vla(3,4,a)    = %d\n", sum_vla(3, 4, a));

    /* build an array of row pointers over the same data */
    int *rowp[3] = { a[0], a[1], a[2] };
    printf("(3) row pointers sum_rows(3,4,rowp) = %d\n\n", sum_rows(3, 4, rowp));

    /* see the difference in layout */
    printf("the layouts differ:\n");
    printf("  a      : int[3][4]  — one run of %zu bytes, 0 indirections\n", sizeof a);
    printf("  rowp   : int*[3]    — %zu bytes of pointers + the rows, 1 indirection\n", sizeof rowp);

    /* the cost of swapping rows differs */
    int *tmp = rowp[0]; rowp[0] = rowp[2]; rowp[2] = tmp;  /* only two pointers move */
    printf("\nswap the row pointers and the order changes with the data untouched: ");
    for (size_t j = 0; j < 4; j++) printf("%d ", rowp[0][j]);
    printf("\n  (doing the same on a 2-D array means actually moving 16 bytes)\n");

    /* jagged rows — a layout a 2-D array cannot have */
    int r0[] = { 1 }, r1[] = { 2, 3, 4 };
    int *jag[2] = { r0, r1 };
    size_t len[2] = { 1, 3 };
    printf("\njagged rows: ");
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < len[i]; j++) printf("%d ", jag[i][j]);
    printf("\n");
    return 0;
}
