#include <stdio.h>

void try_change(int x)
{
    x = 999;                    /* 복사본을 바꿀 뿐이다 */
    printf("함수 안의 x: %d\n", x);
}

int main(void)
{
    int n = 1;

    try_change(n);
    printf("함수 뒤의 n: %d\n", n);   /* 원본은 그대로다 */
    return 0;
}
