#include <limits.h>
#include <stdio.h>

int main(void)
{
    printf("INT_MAX  = %d\n", INT_MAX);
    printf("UINT_MAX = %u\n", UINT_MAX);
    printf("UINT_MAX + 1 = %u\n", UINT_MAX + 1u);  /* defined wrap-around */
    return 0;
}
