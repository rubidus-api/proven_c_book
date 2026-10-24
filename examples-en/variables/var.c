#include <stdio.h>

int main(void)
{
    int apples = 12;              /* declared and initialised at once */

    printf("apples: %d\n", apples);
    apples = apples + 3;          /* the new value = the value now + 3 */
    printf("apples: %d\n", apples);
    return 0;
}
