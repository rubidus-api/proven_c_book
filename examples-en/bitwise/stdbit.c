// C23 gave the idioms names --- <stdbit.h>.
// Put the hand-written version and the standard function side by side and check.
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

static unsigned width_by_hand(uint32_t x)          // bits needed to hold x
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

    // The position-finding functions count from 1, and 0 means "there is none".
    printf("first_leading_one(0x00F0) = %u  (counted from the top, 1-based)\n",
           (unsigned)stdc_first_leading_one(UINT32_C(0x00F0)));
    printf("first_trailing_one(0x00F0) = %u  (counted from the bottom)\n",
           (unsigned)stdc_first_trailing_one(UINT32_C(0x00F0)));
    printf("first_trailing_one(0) = %u  (zero means 'there is none')\n",
           (unsigned)stdc_first_trailing_one(UINT32_C(0)));

    // The power-of-two test got a name too.
    printf("has_single_bit(1024) = %d, has_single_bit(1000) = %d\n",
           (int)stdc_has_single_bit(UINT32_C(1024)),
           (int)stdc_has_single_bit(UINT32_C(1000)));
    return 0;
}
