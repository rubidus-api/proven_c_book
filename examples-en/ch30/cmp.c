#include <stdio.h>

int main(void)
{
    bool tall = 10 > 3;                 /* the result of a comparison is a bool */
    printf("%d\n", tall);
    printf("%d\n", 3 < 5 && 2 + 2 == 4);

    /* short-circuit: if the left side is 0 (false), the right side is never evaluated */
    printf("%d\n", 0 && printf("this line is never printed\n"));
    return 0;
}
