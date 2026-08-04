#include <stdio.h>

int main(void)
{
    int apples = 12;              /* 선언과 동시에 초기화한다 */

    printf("사과: %d개\n", apples);
    apples = apples + 3;          /* 새 값 = 지금 값 + 3 */
    printf("사과: %d개\n", apples);
    return 0;
}
