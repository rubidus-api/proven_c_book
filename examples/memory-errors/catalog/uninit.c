/* 초기화를 어긴 읽기 --- 한 갈래에서만 값을 넣고, 늘 읽는다. */
#include <stdio.h>

[[gnu::noinline]] static int pick(int *p, int c)
{
    if (c) *p = 1;
    return *p;              /* c 가 0 이면 불확정 값을 읽는다 */
}

int main(int argc, char **argv)
{
    (void)argv;
    int x;
    if (pick(&x, argc > 5)) puts("yes");
}
