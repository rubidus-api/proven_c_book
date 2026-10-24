#include <stdio.h>

void try_change(int x)
{
    x = 999;                    /* it only changes the copy */
    printf("inside function, x = %d\n", x);
}

int main(void)
{
    int n = 1;

    try_change(n);
    printf("after the call,  n = %d\n", n);   /* the original is untouched */
    return 0;
}
