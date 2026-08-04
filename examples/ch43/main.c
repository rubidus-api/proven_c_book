/* 여러 파일 예제 — 이 파일은 greet의 선언만 보고 컴파일된다. */
#include "greet.h"

int main(void)
{
    greet("세상");
    printf_count();
    return 0;
}
