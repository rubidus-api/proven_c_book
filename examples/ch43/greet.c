#include "greet.h"
#include <stdio.h>

static int calls = 0;    /* static: 이 파일 안에서만 보이는 이름 */

void greet(const char *who)
{
    calls += 1;
    printf("안녕, %s!\n", who);
}

void printf_count(void)
{
    printf("greet를 %d번 불렀다\n", calls);
}
