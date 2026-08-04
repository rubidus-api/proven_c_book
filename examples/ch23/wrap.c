#include <limits.h>
#include <stdio.h>

int main(void)
{
    printf("int의 최댓값:      %d\n", INT_MAX);
    printf("unsigned의 최댓값: %u\n", UINT_MAX);
    printf("거기에 + 1을 하면: %u\n", UINT_MAX + 1u);  /* 정의된 감아 돌기 */
    return 0;
}
