#include <stdio.h>
#include <stdlib.h>

/* 계약: divisor가 0이 아니어야 한다. 실패는 반환값으로 알린다. */
[[nodiscard]] bool safe_div(int a, int b, int *out)
{
    if (b == 0) {
        return false;           /* 실패 — out은 건드리지 않는다 */
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
