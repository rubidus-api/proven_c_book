/* 프로그램의 기억이 실제로 어디에 놓이는지 주소로 확인한다.
   (구체적 값은 기계·운영체제·실행마다 다르다. 보아야 할 것은 *구역의 순서*다.) */
#include <stdio.h>
#include <stdlib.h>

const char  ro_text[]  = "read-only";   /* 상수 — 대개 읽기 전용 구역 */
int         initialized = 7;             /* 값이 있는 전역 — data      */
int         zeroed;                      /* 값이 없는 전역 — bss       */

static void deeper(int depth, char *outer)
{
    char here;                            /* 이 프레임의 지역 변수 */
    if (depth == 0) {
        printf("  a local one frame deeper : %p\n", (void *)&here);
        printf("  difference from the outer frame     : %+ld bytes\n",
               (long)(&here - outer));
        printf("  => the stack grows toward %s addresses\n",
               (&here < outer) ? "lower" : "higher");
        return;
    }
    deeper(depth - 1, outer);
}

int main(void)
{
    static int  static_local;             /* 지역이지만 정적 저장 기간 */
    int         automatic = 1;            /* 자동 저장 기간 — 스택     */
    void       *heap1 = malloc(64);
    void       *heap2 = malloc(64);

    printf("code (function main)      : %p\n", (void *)(void (*)(void))main);
    printf("read-only string          : %p\n", (void *)ro_text);
    printf("global (initialized, data): %p\n", (void *)&initialized);
    printf("global (zero, bss)        : %p\n", (void *)&zeroed);
    printf("static inside a function  : %p\n", (void *)&static_local);
    printf("heap (malloc 1)           : %p\n", heap1);
    printf("heap (malloc 2)           : %p\n", heap2);
    printf("stack (a local)           : %p\n", (void *)&automatic);
    printf("\ngap between the two heap blocks: %+ld bytes\n", (long)((char *)heap2 - (char *)heap1));
    printf("distance from stack to heap    : about %.1f TiB (a 64-bit address space is that wide)\n\n",
           ((double)((char *)&automatic - (char *)heap1)) / (1024.0 * 1024.0 * 1024.0 * 1024.0));

    char anchor;                          /* 스택 방향을 재는 기준점 */
    deeper(1, &anchor);

    free(heap1);
    free(heap2);
    return 0;
}
