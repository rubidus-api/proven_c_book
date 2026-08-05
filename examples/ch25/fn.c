#include <stdio.h>

int square(int n)     /* 정수 하나를 받아 정수 하나를 돌려주는 일꾼 */
{
    return n * n;
}

int main(void)
{
    printf("%d\n", square(12));
    return 0;
}
