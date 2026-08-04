#include <stdckdint.h>
#include <stdio.h>

/* C23의 검사 산술: 넘치면 true를 돌려주고, 결과는 감아 돈 값이 담긴다. */
int main(void)
{
    int a = 2000000000;
    int b = 2000000000;
    int sum = 0;

    if (ckd_add(&sum, a, b)) {
        printf("%d + %d 는 int 그릇을 넘친다 (계약 밖을 피했다)\n", a, b);
    } else {
        printf("합: %d\n", sum);
    }

    int small = 0;
    if (ckd_add(&small, 20, 22)) {
        printf("넘침\n");
    } else {
        printf("20 + 22 = %d\n", small);
    }
    return 0;
}
