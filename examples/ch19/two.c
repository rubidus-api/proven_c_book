// 한 문장씩, 위에서 아래로 차례로 실행된다.
#include <stdio.h>

int main(void)
{
    printf("Hello, ");   /* 줄바꿈이 없다 — 다음 출력이 이어 붙는다 */
    printf("world!\n");  /* 줄바꿈은 여기, \n에서만 일어난다 */
    return 0;
}
