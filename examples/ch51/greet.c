#include "greet.h"
#include <stdio.h>

static int calls = 0;    /* static: 이 파일 안에서만 보이는 이름 */

void greet(const char *who)
{
    calls += 1;
    printf("Hello, %s!\n", who);
}

void printf_count(void)
{
    printf("greet was called %d times\n", calls);
}
