#include <stdio.h>

/* The three declarations are exactly the same thing to the compiler: all int *.
   (Applying sizeof directly to a parameter name makes gcc warn, so we take it
    into a local variable of the same type once and measure that.) */
static void by_ptr(int *a)     { int *p = a; printf("  int *a      : sizeof = %zu\n", sizeof p); }
static void by_arr(int a[])    { int *p = a; printf("  int a[]     : sizeof = %zu\n", sizeof p); }
static void by_size(int a[10]) { int *p = a; printf("  int a[10]   : sizeof = %zu\n", sizeof p); }

/* A parameter is just a pointer variable, so another address can be assigned to it */
static int second_of(int a[10])
{
    a = a + 1;          /* impossible for an array — an array name cannot be assigned to */
    return a[0];
}

/* Two dimensions: only the outermost (leftmost) dimension is stripped.
   int m[3][4]  ->  int (*m)[4] */
static void by_2d(int m[3][4])
{
    int (*p)[4] = m;    /* this is the parameter's real type */
    printf("  int m[3][4] : sizeof = %zu (a pointer), sizeof p[0] = %zu (one row)\n",
           sizeof p, sizeof p[0]);
    printf("                m[1][2] = %d\n", m[1][2]);
}

int main(void)
{
    int arr[10] = {0,1,2,3,4,5,6,7,8,9};
    int grid[3][4] = { {0,1,2,3}, {4,5,6,7}, {8,9,10,11} };

    printf("on the caller's side (real arrays):\n");
    printf("  int arr[10]    : sizeof = %zu\n", sizeof arr);
    printf("  int grid[3][4] : sizeof = %zu, sizeof grid[0] = %zu\n",
           sizeof grid, sizeof grid[0]);

    printf("inside the functions (all pointers):\n");
    by_ptr(arr);
    by_arr(arr);
    by_size(arr);
    by_2d(grid);

    printf("assigning to a parameter: second_of(arr) = %d\n", second_of(arr));
    printf("the original is untouched: arr[0] = %d\n", arr[0]);
    return 0;
}
