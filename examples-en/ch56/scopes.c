/* The four scopes a name can have --- exactly as the standard defines them. */
#include <stdio.h>

/* (1) file scope: visible from here to the end of this file */
static int file_level = 1;

/* (2) function prototype scope: the parameter names rows and cols vanish when
      this parenthesis closes. They are useful even so --- the size of a later
      parameter is written with them. */
void print_grid(int rows, int cols, int grid[rows][cols]);

/* (3) function scope: a label is visible throughout the function, even one written inside a block. */
static int find_first_negative(const int *a, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] < 0) {
            goto found;          /* jump to a label inside the block below */
        }
    }
    return -1;
    {
    found:                       /* inside a block, yet visible anywhere in the function */
        return 0;
    }
}

void print_grid(int rows, int cols, int grid[rows][cols])
{
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) printf(" %d", grid[r][c]);
        putchar('\n');
    }
}

int main(void)
{
    /* (4) block scope: only inside these braces */
    int block_level = 2;
    {
        int inner = 3;
        printf("inner block sees: file=%d block=%d inner=%d\n",
               file_level, block_level, inner);
    }
    /* out here, inner is no longer a name */

    int grid[2][3] = {{1, 2, 3}, {4, 5, 6}};
    puts("\nthe grid, sized by names from the prototype scope:");
    print_grid(2, 3, grid);

    int values[] = {5, 7, -2, 9};
    printf("\nfirst negative found: %s\n",
           find_first_negative(values, 4) == 0 ? "yes" : "no");
    return 0;
}
