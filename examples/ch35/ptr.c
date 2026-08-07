#include <stdio.h>

void set_to(int *target, int value)
{
    *target = value;            /* 주소를 따라가 원본에 쓴다 */
}

int main(void)
{
    int n = 1;
    int *p = &n;                /* p는 n의 주소를 담는다 */

    printf("n at first: %d\n", n);
    printf("p == &n ? %d\n", p == &n);

    *p = 55;                    /* 역참조: p가 가리키는 곳(= n)에 쓴다 */
    printf("n after *p = 55: %d\n", n);

    set_to(&n, 99);             /* 29장의 숙제 — 함수가 원본을 바꾼다 */
    printf("n after set_to: %d\n", n);
    return 0;
}
