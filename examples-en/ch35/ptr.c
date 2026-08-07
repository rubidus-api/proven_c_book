#include <stdio.h>

void set_to(int *target, int value)
{
    *target = value;            /* it follows the address and writes to the original */
}

int main(void)
{
    int n = 1;
    int *p = &n;                /* p holds the address of n */

    printf("n at first: %d\n", n);
    printf("p == &n ? %d\n", p == &n);

    *p = 55;                    /* dereference: write where p points (= n) */
    printf("n after *p = 55: %d\n", n);

    set_to(&n, 99);             /* chapter 29's homework — a function changes the original */
    printf("n after set_to: %d\n", n);
    return 0;
}
