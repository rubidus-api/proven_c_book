/* 초기화를 어긴 읽기 --- bool 의 자리에 0 도 1 도 아닌 바이트(2)를 넣고 읽는다. */
#include <string.h>

[[gnu::noinline]] static bool get(const bool *p) { return *p; }

int main(void)
{
    bool b;
    unsigned char two = 2;
    memcpy(&b, &two, 1);    /* 바이트로 쓰는 것까지는 계약 안이다 */
    return get(&b) ? 0 : 1; /* bool 로 읽는 순간 비값 표현을 읽는다(C23 6.2.6.1p5) */
}
