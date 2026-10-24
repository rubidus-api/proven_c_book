/* Shadowing --- and the fact that the preprocessor knows nothing of scope. */
#include <stdio.h>

int count = 100;                 /* file scope */

static void layers(void)
{
    printf("file scope        : count = %d\n", count);
    int count = 10;              /* shadows the file-scope count */
    printf("function body     : count = %d\n", count);
    {
        int count = 1;           /* shadows it again */
        printf("inner block       : count = %d\n", count);
    }
    printf("back in the body  : count = %d\n", count);
    /* a shadowed name is not gone --- it is merely covered for a while */
}

static void define_inside(void)
{
    /* This directive has nothing to do with being "inside a function".
       Preprocessing finishes before compiling, and it knows no blocks. */
#define LIMIT 5
    printf("\ninside the function : LIMIT = %d\n", LIMIT);
}

static void far_away(void)
{
    /* A different function, and LIMIT is still alive --- its reach is not a
       scope but "from that point to the end of the file". */
    printf("another function    : LIMIT = %d\n", LIMIT);
}

int main(void)
{
    layers();
    define_inside();
    far_away();
    printf("\nthe file-scope count is still %d\n", count);
    return 0;
}
