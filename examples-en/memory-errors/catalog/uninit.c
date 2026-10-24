/* Breaking initialisation, reading --- store a value on one branch only, and always read. */
#include <stdio.h>

[[gnu::noinline]] static int pick(int *p, int c)
{
    if (c) *p = 1;
    return *p;              /* if c is 0 this reads an indeterminate value */
}

int main(int argc, char **argv)
{
    (void)argv;
    int x;
    if (pick(&x, argc > 5)) puts("yes");
}
