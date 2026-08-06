#include <stdio.h>

int main(void)
{
    bool tall = 10 > 3;                 /* 비교의 결과는 bool 값이다 */
    printf("%d\n", tall);
    printf("%d\n", 3 < 5 && 2 + 2 == 4);

    /* 단락 평가: 왼쪽이 0(거짓)이면 오른쪽은 아예 평가되지 않는다 */
    printf("%d\n", 0 && printf("this line is never printed\n"));
    return 0;
}
