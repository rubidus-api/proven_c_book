/* 버퍼 오버플로 --- 8바이트 그릇에 17바이트(널 포함)를 strcpy 한다.
 * 원본 문자열을 실행 때 고르게 해서, 컴파일러가 미리 알아채지 못하게 했다. */
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    (void)argv;
    char name[8];
    const char *src = argc > 5 ? "ok" : "0123456789abcdef";
    strcpy(name, src);              /* 그릇의 크기를 받지 않는 함수 */
    printf("copied %zu bytes into an 8-byte buffer\n", strlen(name) + 1);
    return 0;
}
