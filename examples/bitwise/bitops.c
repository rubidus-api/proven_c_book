// 비트를 다루는 다섯 가지 기본 동작.
// 모두 부호 없는 타입 위에서, 폭을 이름에 적은 타입으로 한다.
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

static void show(const char *label, uint32_t v)
{
    printf("  %-28s 0x%08" PRIX32 "\n", label, v);
}

int main(void)
{
    uint32_t v = 0x00F0'0000;          // C23 의 자릿수 구분자
    show("start", v);

    // ① 세우기 --- 그 자리에 1 을 넣는다
    v |= UINT32_C(1) << 3;
    show("set bit 3        (|=)", v);

    // ② 지우기 --- 그 자리에만 0 인 마스크와 AND
    v &= ~(UINT32_C(1) << 20);
    show("clear bit 20     (&= ~)", v);

    // ③ 읽기 --- 결과는 0 이거나 「그 비트의 값」이지 1 이 아니다
    printf("  is bit 3 on?  %s\n", (v & (UINT32_C(1) << 3)) ? "yes" : "no");
    printf("  the value of v & (1<<3) is %" PRIu32 ", not 1\n",
           v & (UINT32_C(1) << 3));

    // ④ 뒤집기 --- XOR 은 마스크가 1 인 자리만 뒤집는다
    v ^= UINT32_C(0xF0);
    show("flip bits 4..7   (^=)", v);

    // ⑤ 필드 바꿔 넣기 --- 지우고, 밀어 넣는다
    // 비트 8~15 를 하나의 8비트 필드로 본다.
    uint32_t field = 0xAB;
    uint32_t mask  = UINT32_C(0xFF) << 8;
    v = (v & ~mask) | ((field << 8) & mask);
    show("put 0xAB into bits 8..15", v);

    // 꺼내는 것은 반대 순서다 --- 내리고, 남긴다
    printf("  reading it back gives 0x%02" PRIX32 "\n", (v >> 8) & 0xFF);
    return 0;
}
