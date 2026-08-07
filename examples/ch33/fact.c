#include <stdio.h>

int fact(int n)
{
    if (n <= 1) {
        return 1;               /* 바닥: 더 내려가지 않는 경우 */
    }
    return n * fact(n - 1);     /* 자신을 부르되, 더 작은 문제로 */
}

int main(void)
{
    printf("5! = %d\n", fact(5));
    return 0;
}
