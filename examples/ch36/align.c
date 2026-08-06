#include <stdio.h>

int main(void)
{
    printf("alignof(char)   = %zu\n", alignof(char));
    printf("alignof(int)    = %zu\n", alignof(int));
    printf("alignof(double) = %zu\n", alignof(double));
    return 0;
}
