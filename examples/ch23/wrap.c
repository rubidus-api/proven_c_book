#include <limits.h>
#include <stdio.h>

int main(void)
{
    printf("INT_MAX  = %d\n", INT_MAX);
    printf("UINT_MAX = %u\n", UINT_MAX);
    printf("UINT_MAX + 1 = %u\n", UINT_MAX + 1u);  /* 정의된 감아 돌기 */
    return 0;
}
