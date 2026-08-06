/* 프로그램의 기억이 실제로 어디에 놓이는지 주소로 확인한다.
   (구체적 값은 기계·운영체제·실행마다 다르다. 보아야 할 것은 *구역의 순서*다.) */
#include <stdio.h>
#include <stdlib.h>

const char  ro_text[]  = "읽기 전용";   /* 상수 — 대개 읽기 전용 구역 */
int         initialized = 7;             /* 값이 있는 전역 — data      */
int         zeroed;                      /* 값이 없는 전역 — bss       */

static void deeper(int depth, char *outer)
{
    char here;                            /* 이 프레임의 지역 변수 */
    if (depth == 0) {
        printf("  한 단계 더 들어간 프레임의 지역 변수 : %p\n", (void *)&here);
        printf("  바깥 프레임과의 차이               : %+ld 바이트\n",
               (long)(&here - outer));
        printf("  => 스택은 주소가 %s 방향으로 자란다\n",
               (&here < outer) ? "작아지는" : "커지는");
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

    printf("코드(함수 main)        : %p\n", (void *)(void (*)(void))main);
    printf("읽기 전용 문자열       : %p\n", (void *)ro_text);
    printf("전역 (값 있음, data)   : %p\n", (void *)&initialized);
    printf("전역 (값 없음, bss)    : %p\n", (void *)&zeroed);
    printf("함수 안 static         : %p\n", (void *)&static_local);
    printf("힙 (malloc 1)          : %p\n", heap1);
    printf("힙 (malloc 2)          : %p\n", heap2);
    printf("스택 (지역 변수)       : %p\n", (void *)&automatic);
    printf("\n힙 두 블록의 간격      : %+ld 바이트\n", (long)((char *)heap2 - (char *)heap1));
    printf("스택과 힙의 거리       : 약 %.1f TiB (64비트 주소 공간은 이렇게 넓다)\n\n",
           ((double)((char *)&automatic - (char *)heap1)) / (1024.0 * 1024.0 * 1024.0 * 1024.0));

    char anchor;                          /* 스택 방향을 재는 기준점 */
    deeper(1, &anchor);

    free(heap1);
    free(heap2);
    return 0;
}
