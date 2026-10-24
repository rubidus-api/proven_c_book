/* The rewrite of array parameters into pointers applies to ONE layer only.
   And since C99 you can pass the sizes and get a real 2-D parameter. */
#include <stdio.h>

/* (1) array of array -> pointer TO ARRAY  (not pointer to pointer) */
static void take_2d(int (*m)[3], size_t rows)
{
    printf("  int m[][3]  -> int (*m)[3] : sizeof m = %zu, sizeof *m = %zu\n",
           sizeof m, sizeof *m);
    printf("  m[1][2] = %d   (one row is %zu bytes)\n", m[1][2], sizeof *m);
    (void)rows;
}

/* (2) array of pointer -> pointer to pointer  (argv has this shape) */
static void take_argvish(char **v)
{
    printf("  char *v[]   -> char **v   : v[0]=\"%s\", v[1]=\"%s\"\n", v[0], v[1]);
}

/* (3) since C99 - take the sizes first and the 2-D parameter really varies */
static long sum_2d(size_t rows, size_t cols, int m[rows][cols])
{
    long s = 0;
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
            s += m[i][j];
    return s;
}

int main(void)
{
    int  grid[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
    int  wide[2][4] = { { 1, 1, 1, 1 }, { 2, 2, 2, 2 } };
    char *names[]   = { "hana", "dul" };

    puts("(1) pass an array of arrays");
    take_2d(grid, 2);

    puts("\n(2) pass an array of pointers");
    take_argvish(names);

    puts("\n(3) pass the sizes and one function takes both widths");
    printf("  sum_2d(2,3,grid) = %ld\n", sum_2d(2, 3, grid));
    printf("  sum_2d(2,4,wide) = %ld   <- a shape take_2d cannot accept\n",
           sum_2d(2, 4, wide));

    return 0;
}
