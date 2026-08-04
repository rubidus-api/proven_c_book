#include <stdio.h>

int main(void)
{
    int a[5] = {3, 1, 4, 1, 5};
    int sum = 0;

    for (int i = 0; i < 5; i += 1) {
        sum += a[i];
    }
    printf("합: %d\n", sum);

    printf("a[2] = %d, *(a + 2) = %d\n", a[2], *(a + 2));

    int *p = a;                 /* 배열 이름이 첫 원소의 주소로 무너진다 */
    printf("sizeof a = %zu, sizeof p = %zu\n", sizeof a, sizeof p);
    return 0;
}
