#include "greet.h"
#include <stdio.h>

static int calls = 0;    /* static: a name visible only inside this file */

void greet(const char *who)
{
    calls += 1;
    printf("Hello, %s!\n", who);
}

void printf_count(void)
{
    printf("greet was called %d times\n", calls);
}
