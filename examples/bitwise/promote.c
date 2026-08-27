// 비트 연산에서 가장 자주 데이는 자리 --- 정수 승격.
// int 보다 좁은 타입은 연산 전에 int 로 넓혀진다. 그래서 「8비트를 뒤집었다」고
// 생각한 결과가 32비트짜리로 나온다.
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int main(void)
{
    uint8_t c = 0x0F;

    // ~c 의 타입은 uint8_t 가 아니라 int 다.
    printf("c             = 0x%02X\n", c);
    printf("~c            = 0x%08X   <- not eight bits\n", (unsigned)~c);
    printf("(uint8_t)~c   = 0x%02X         <- narrow it back yourself\n",
           (unsigned)(uint8_t)~c);
    // 여기서 (~c == 0xF0) 이라 적으면 gcc 가 -Wsign-compare 로 막아 준다.
    printf("((uint8_t)~c == 0xF0) is %s\n", ((uint8_t)~c == 0xF0) ? "true" : "false");

    // char 가 부호 있는 기계에서 0x80 이상인 바이트를 int 로 넓히면 음수가 된다.
    // 그래서 바이트를 다룰 때는 unsigned char 로 받는다.
    char signed_byte = (char)0x80;
    unsigned char plain_byte = 0x80;
    printf("a char holding 0x80, widened  : %d\n", (int)signed_byte);
    printf("an unsigned char holding 0x80 : %d\n", (int)plain_byte);
    printf("masking with 0xFF fixes it    : %d\n", (int)(signed_byte & 0xFF));

    // 마스크의 폭도 타입을 따라간다.
    uint64_t wide = 0xFFFF'FFFF'FFFF'FFFFu;
    printf("wide & ~0u          = 0x%016" PRIX64 "   <- ~0u is 32 bits wide\n",
           wide & ~0u);
    printf("wide & ~UINT64_C(0) = 0x%016" PRIX64 "\n", wide & ~UINT64_C(0));
    return 0;
}
