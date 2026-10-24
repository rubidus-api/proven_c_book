/* 수명을 어긴 해제 ② --- 빌린 적 없는 주소(블록의 가운데)를 돌려준다. */
#include <stdlib.h>

[[gnu::noinline]] static void release(void *p) { free(p); }

int main(void)
{
    char *p = malloc(16);
    if (p == NULL) return 1;
    release(p + 4);
}
