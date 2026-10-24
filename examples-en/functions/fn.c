#include <stdio.h>

int square(int n)     /* a worker taking one integer and returning one integer */
{
    return n * n;
}

int main(void)
{
    printf("%d\n", square(12));
    return 0;
}
