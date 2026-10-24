/* 타입을 어긴 읽기 --- 4바이트 정렬이 필요한 uint32_t 를 홀수 주소에서 읽는다. */
#include <stdint.h>

[[gnu::noinline]] static uint32_t read32(const unsigned char *p)
{
    return *(const uint32_t *)p;    /* 변환 자체가 계약 밖이다(C23 6.3.2.3p7) */
}

int main(void)
{
    alignas(4) unsigned char b[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    return (int)(read32(b + 1) & 1);
}
