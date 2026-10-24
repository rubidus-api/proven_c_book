#include <stdio.h>

int main(void)
{
    printf("%d\n", 2 + 3 * 4);    /* the multiplication is computed first */
    printf("%d\n", (2 + 3) * 4);  /* the parentheses state the order */
    return 0;
}
