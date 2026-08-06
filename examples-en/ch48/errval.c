#include <stdio.h>
#include <stdlib.h>

/* The contract: the divisor must not be 0. Failure is reported by the return value. */
[[nodiscard]] bool safe_div(int a, int b, int *out)
{
    if (b == 0) {
        return false;           /* failure — out is left untouched */
    }
    *out = a / b;
    return true;
}

int main(void)
{
    int result = 0;

    if (safe_div(7, 2, &result)) {
        printf("7 / 2 = %d\n", result);
    } else {
        printf("7 / 2: failed\n");
    }

    if (safe_div(7, 0, &result)) {
        printf("7 / 0 = %d\n", result);
    } else {
        printf("7 / 0: cannot divide by zero (reported as a value)\n");
    }
    return 0;
}
