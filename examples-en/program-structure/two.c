// One statement at a time, top to bottom, in order.
#include <stdio.h>

int main(void)
{
    printf("Hello, ");   /* no newline — the next output joins on */
    printf("world!\n");  /* the line break happens here, at \n, and nowhere else */
    return 0;
}
