#include <stdio.h>

int main(void) {
    int n = 42;
    unsigned u = 255;
    double x = 3.14159265;
    const char *s = "proven";
    size_t sz = 1234;
    long long big = 9000000000LL;

    /* 플래그와 폭: 오른쪽 정렬이 기본, - 는 왼쪽, 0 은 빈칸 대신 0 */
    printf("[%d] [%6d] [%-6d] [%06d] [%+d]\n", n, n, n, n, n);

    /* 정밀도: 실수는 소수 자릿수, 문자열은 최대 길이 */
    printf("[%f] [%.2f] [%10.3f] [%e] [%g]\n", x, x, x, x, x);
    printf("[%s] [%10s] [%-10s] [%.3s]\n", s, s, s, s);

    /* 진법과 문자 */
    printf("[%c] [%x] [%X] [%#x] [%o] [%u]\n", 'A', u, u, u, u, u);

    /* 길이 수식어: 인자의 폭을 서식에 알려 준다 */
    printf("[%zu] [%lld]\n", sz, big);

    /* 폭·정밀도를 인자로 넘기기 */
    printf("[%.*s] [%*d]\n", 4, s, 6, n);

    /* 퍼센트 글자 자체 */
    printf("100%%\n");
    return 0;
}
