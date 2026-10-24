/* 수명을 어긴 해제 ① --- 같은 블록을 두 번 돌려준다. */
#include <stdlib.h>

[[gnu::noinline]] static void release(void *p) { free(p); }

int main(void)
{
    void *p = malloc(16);
    release(p);
    release(p);
}
