#include <stdio.h>

int main(void)
{
    int sum = 0;

    for (int i = 1; i <= 100; i += 1) {
        sum += i;               /* 불변식: sum == 1 + 2 + ... + (i - 1)... 갱신 후엔 ...+ i */
    }
    printf("sum of 1..100 = %d\n", sum);
    return 0;
}
