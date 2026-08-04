#include <stdio.h>

void set_to(int *target, int value)
{
    *target = value;            /* 주소를 따라가 원본에 쓴다 */
}

int main(void)
{
    int n = 1;
    int *p = &n;                /* p는 n의 주소를 담는다 */

    printf("처음의 n: %d\n", n);
    printf("p == &n ? %d\n", p == &n);

    *p = 55;                    /* 역참조: p가 가리키는 곳(= n)에 쓴다 */
    printf("*p = 55 후의 n: %d\n", n);

    set_to(&n, 99);             /* 28장의 숙제 — 함수가 원본을 바꾼다 */
    printf("set_to 후의 n: %d\n", n);
    return 0;
}
