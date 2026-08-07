#include <stdint.h>
#include <stdio.h>

int main(void)
{
    /* 정수 승격: char·short는 계산 전에 int로 넓혀진다 */
    signed char a = 100;
    signed char b = 100;
    printf("100 + 100 (as chars) = %d\n", a + b);   /* 200 — char에는 안 담기는 값 */

    /* 통상 산술 변환: 부호 없는 쪽이 이긴다 —
       -1을 unsigned의 눈으로 읽으면 거대한 양수가 된다 */
    int neg = -1;
    printf("(unsigned)(-1)   = %u\n", (unsigned)neg);
    printf("so -1 < 1u is false\n");

    /* 정수 나눗셈 vs 실수 나눗셈 — 캐스트로 의도를 밝힌다 */
    int total = 7, count = 2;
    printf("7 / 2        = %d\n", total / count);
    printf("(double)7/2  = %.1f\n", (double)total / count);

    /* 가변 인자의 기본 진급: float는 double로, char/short는 int로 */
    float f = 1.5f;
    printf("float 1.5f via %%f = %f  (promoted to double)\n", f);
    return 0;
}
