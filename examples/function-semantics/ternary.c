#include <stdio.h>

int main(void)
{
    int    i = 7;
    double d = 2.5;

    /* 조건 연산자의 타입은 *컴파일 시간에 하나로* 정해진다.
       두 가지가 int 와 double 이면 결과는 언제나 double 이다. */
    printf("sizeof(int)=%zu sizeof(double)=%zu\n", sizeof i, sizeof d);
    printf("sizeof(1 ? i : d) = %zu  (double even when the condition is true)\n", sizeof(1 ? i : d));
    printf("value  (1 ? i : d) = %.1f\n", 1 ? i : d);   /* 7 이 아니라 7.0 */

    /* 부호가 섞이면 통상 산술 변환이 그대로 적용된다.
       (아래는 컴파일러 경고를 피하려고 미리 맞춰 준 판이다 —
        맞추지 않으면 gcc 가 -Wsign-compare 로 이 자리를 짚는다) */
    int      neg = -1;
    unsigned one = 1u;
    unsigned mixed = 1 ? (unsigned)neg : one;
    printf("(1 ? neg : one) becomes unsigned = %u\n", mixed);

    /* 문자 상수는 C 에서 이미 int 다 */
    printf("sizeof('a')=%zu sizeof(1 ? 'a' : 'b')=%zu\n",
           sizeof('a'), sizeof(1 ? 'a' : 'b'));

    /* 포인터 쪽 규칙: 한쪽이 널 포인터 상수면 결과는 다른 쪽의 타입 */
    const char *s = "text";
    const char *r = 1 ? s : NULL;
    printf("pointer branch: %s\n", r);
    return 0;
}
