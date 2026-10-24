#include <stdio.h>

int fact(int n)
{
    if (n <= 1) {
        return 1;               /* the floor: the case that goes no deeper */
    }
    return n * fact(n - 1);     /* it calls itself, but with a smaller problem */
}

int main(void)
{
    printf("5! = %d\n", fact(5));
    return 0;
}
