#include "greet.h"
#include <stdio.h>

int main(void)
{
    /* 그릇의 크기도 헤더에서 받는다 --- 두 파일이 같은 수를 믿어야 한다 */
    char buf[GREET_MAX];
    greet(buf);
    printf("  GREET_MAX=%d  len=%zu\n", GREET_MAX, sizeof buf - 1);
    return 0;
}
