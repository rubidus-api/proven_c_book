#include <stdio.h>

int main(void)
{
    char line[100];   /* 글자 100칸짜리 공간 — 정식 설명은 33장 */
    int n = 0;

    fgets(line, sizeof line, stdin);   /* 1단계: 한 줄을 통째로 읽는다 */
    sscanf(line, "%d", &n);            /* 2단계: 그 줄에서 정수를 해석한다 */

    printf("%d squared is %d.\n", n, n * n);
    return 0;
}
