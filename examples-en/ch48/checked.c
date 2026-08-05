#include <stdckdint.h>
#include <stdio.h>

/* C23's checked arithmetic: on overflow it returns true, and the result holds the wrapped value. */
int main(void)
{
    int a = 2000000000;
    int b = 2000000000;
    int sum = 0;

    if (ckd_add(&sum, a, b)) {
        printf("%d + %d overflows int (we stayed inside the contract)\n", a, b);
    } else {
        printf("sum: %d\n", sum);
    }

    int small = 0;
    if (ckd_add(&small, 20, 22)) {
        printf("overflow\n");
    } else {
        printf("20 + 22 = %d\n", small);
    }
    return 0;
}
