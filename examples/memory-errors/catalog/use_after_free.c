/* 수명을 어긴 읽기 --- 돌려준 블록을 다시 읽는다. */
#include <stdlib.h>

[[gnu::noinline]] static int peek(const int *p) { return *p; }

int main(void)
{
    int *p = malloc(sizeof *p);
    if (p == NULL) return 1;
    *p = 7;
    free(p);
    return peek(p);         /* p 의 값은 이제 불확정이다(C23 6.2.4p2) */
}
