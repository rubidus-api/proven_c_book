/* 크기를 어긴 쓰기 ② --- 힙에서 빌린 8바이트, 아홉째 칸에 쓴다. */
#include <stdlib.h>

[[gnu::noinline]] static void put(char *p, size_t i) { p[i] = 'x'; }

int main(void)
{
    char *p = malloc(8);
    if (p == NULL) return 1;
    put(p, 8);
    free(p);
}
