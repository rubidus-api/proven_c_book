#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* The contract.
   Failure: if b is 0 or the quotient cannot be represented in int (INT_MIN / -1), return false
            and leave *out untouched.
   Precondition: out points to a valid int object --- the caller's part, which the function does not check. */
[[nodiscard]] bool safe_div(int a, int b, int *out)
{
    if (b == 0 || (a == INT_MIN && b == -1)) {
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
        printf("7 / 0: cannot divide by zero (reported as a value); result is still %d\n", result);
    }

    if (safe_div(INT_MIN, -1, &result)) {
        printf("INT_MIN / -1 = %d\n", result);
    } else {
        printf("INT_MIN / -1: the quotient does not fit in int (reported as a value); result is still %d\n", result);
    }
    return 0;
}
