#include "greet.h"
#include <stdio.h>

int main(void)
{
    /* The size comes from the header too - both files must trust the same number. */
    char buf[GREET_MAX];
    greet(buf);
    printf("  GREET_MAX=%d  len=%zu\n", GREET_MAX, sizeof buf - 1);
    return 0;
}
