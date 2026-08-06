#include <stdio.h>

int main(void)
{
    printf(" 7 / 2 = %d,  7 %% 2 = %d\n", 7 / 2, 7 % 2);
    printf("-7 / 2 = %d, -7 %% 2 = %d\n", -7 / 2, -7 % 2);
    printf("invariant: (7/2)*2 + 7%%2 = %d\n", (7 / 2) * 2 + 7 % 2);

    printf("5 & 3 = %d, 5 | 3 = %d, 5 ^ 3 = %d\n", 5 & 3, 5 | 3, 5 ^ 3);
    printf("1 << 4 = %d\n", 1 << 4);
    return 0;
}
