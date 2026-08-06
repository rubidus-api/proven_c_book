#include <stdio.h>

int main(void)
{
    printf("%d\n", 2 + 3 * 4);    /* 곱셈이 먼저 계산된다 */
    printf("%d\n", (2 + 3) * 4);  /* 괄호가 순서를 명시한다 */
    return 0;
}
