// C23 이 관용구에 이름을 붙였다 --- <stdbit.h>.
// 손으로 짠 것과 표준 함수를 나란히 놓고 답이 같은지 확인한다.
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbit.h>

static unsigned count_by_hand(uint32_t x)
{
    unsigned n = 0;
    while (x) { x &= x - 1u; n++; }
    return n;
}

static unsigned width_by_hand(uint32_t x)          // 담는 데 필요한 비트 수
{
    unsigned n = 0;
    while (x) { x >>= 1; n++; }
    return n;
}

int main(void)
{
    printf("__STDC_VERSION_STDBIT_H__ = %ld\n", (long)__STDC_VERSION_STDBIT_H__);
    printf("byte order is %s\n",
           __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__ ? "little endian"
           : __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__  ? "big endian"
                                                            : "neither");

    uint32_t samples[] = { 0u, 1u, 0x0000'0B40u, 300u, 0xFFFF'FFFFu };
    puts("value       ones(hand/std)  width(hand/std)  bit_ceil   leading zeros");
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; i++) {
        uint32_t x = samples[i];
        printf("0x%08" PRIX32 "   %2u / %-2u        %2u / %-2u         %-10" PRIu32 " %u\n",
               x,
               count_by_hand(x), (unsigned)stdc_count_ones(x),
               width_by_hand(x), (unsigned)stdc_bit_width(x),
               (uint32_t)stdc_bit_ceil(x),
               (unsigned)stdc_leading_zeros(x));
    }

    // 자리 번호를 세는 함수는 *1 부터* 세고, 0 은 「없다」는 뜻이다.
    printf("first_leading_one(0x00F0) = %u  (counted from the top, 1-based)\n",
           (unsigned)stdc_first_leading_one(UINT32_C(0x00F0)));
    printf("first_trailing_one(0x00F0) = %u  (counted from the bottom)\n",
           (unsigned)stdc_first_trailing_one(UINT32_C(0x00F0)));
    printf("first_trailing_one(0) = %u  (zero means 'there is none')\n",
           (unsigned)stdc_first_trailing_one(UINT32_C(0)));

    // 2 의 거듭제곱 판정도 이름을 얻었다.
    printf("has_single_bit(1024) = %d, has_single_bit(1000) = %d\n",
           (int)stdc_has_single_bit(UINT32_C(1024)),
           (int)stdc_has_single_bit(UINT32_C(1000)));
    return 0;
}
