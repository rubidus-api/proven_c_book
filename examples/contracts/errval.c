#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* 계약.
   실패: b 가 0 이거나 몫을 int 로 나타낼 수 없으면(INT_MIN / -1) false 를 돌려주고
         *out 은 건드리지 않는다.
   전제: out 은 유효한 int 객체를 가리킨다 --- 이것은 함수가 검사하지 않는 호출자의 몫이다. */
[[nodiscard]] bool safe_div(int a, int b, int *out)
{
    if (b == 0 || (a == INT_MIN && b == -1)) {
        return false;           /* 실패 — out 은 그대로 */
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
