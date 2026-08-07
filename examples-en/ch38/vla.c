#include <stdio.h>

/* [static N]: the promise "an array of at least N elements comes here".
   It can be used only in an array declarator of a function parameter. */
static int sum3(const int a[static 3])
{
    return a[0] + a[1] + a[2];
}

/* A variable length array (VLA) as a parameter: take the size first, use it after */
static int sum_n(size_t n, const int a[n])
{
    int s = 0;
    for (size_t i = 0; i < n; i++) s += a[i];
    return s;
}

/* A two-dimensional VLA parameter — no computing indices by hand */
static int trace(size_t n, const int m[n][n])
{
    int s = 0;
    for (size_t i = 0; i < n; i++) s += m[i][i];
    return s;
}

int main(void)
{
    int fixed[3] = {1, 2, 3};
    printf("sum3        = %d\n", sum3(fixed));

    size_t n = 4;
    int local[n];                    /* a local VLA: its size is settled while running */
    for (size_t i = 0; i < n; i++) local[i] = (int)(i * i);

    printf("sizeof local = %zu bytes (= %zu ints)\n", sizeof local, sizeof local / sizeof local[0]);
    printf("sum_n       = %d\n", sum_n(n, local));

    int grid[3][3] = { {1,2,3}, {4,5,6}, {7,8,9} };
    printf("trace       = %d\n", trace(3, grid));
    return 0;
}
