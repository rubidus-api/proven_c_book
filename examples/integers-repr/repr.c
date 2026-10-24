/* 수가 비트로 어떻게 담기는가 --- 눈으로 본다.
   2의 보수, 시계처럼 감아 도는 부호 없는 수, 그리고 부호 확장. */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

/* 값의 비트를 높은 자리부터 찍는다 --- 여덟 자리마다 한 칸 띄운다 */
static void bits(const char *label, uint32_t v, int width)
{
    printf("%-24s ", label);
    for (int i = width - 1; i >= 0; i--) {
        putchar((v >> i & 1) ? '1' : '0');
        if (i % 8 == 0 && i != 0) putchar(' ');
    }
    putchar('\n');
}

int main(void)
{
    puts("== two's complement: negative is not a sign bit glued on ==");
    int8_t a = 5, b = -5;
    bits("(int8_t) 5", (uint8_t)a, 8);
    bits("(int8_t) -5", (uint8_t)b, 8);
    bits("flip the bits of 5", (uint8_t)~(uint8_t)a, 8);
    bits("... and add one", (uint8_t)(~(uint8_t)a + 1u), 8);
    printf("so -5 is stored as %u when read as unsigned\n\n", (unsigned)(uint8_t)b);

    puts("== unsigned arithmetic goes round like a clock ==");
    uint8_t clock = 250;
    printf("250 + 10 in a uint8_t = %u   (not 260: it wrapped at 256)\n", (uint8_t)(clock + 10u));
    printf("0 - 1  in a uint8_t   = %u   (the clock ran backwards)\n\n", (uint8_t)(0u - 1u));

    puts("== the same bits mean different numbers ==");
    uint8_t raw = 0xF6;
    printf("bits 11110110 as unsigned = %u\n", (unsigned)raw);
    printf("bits 11110110 as signed   = %d\n\n", (int)(int8_t)raw);

    puts("== sign extension: widening keeps the value, not the bits ==");
    int8_t small = -10;
    int32_t wide = small;
    bits("(int8_t) -10", (uint8_t)small, 8);
    bits("widened to int32_t", (uint32_t)wide, 32);
    printf("value stayed %d --- the machine copied the top bit to fill\n", wide);
    return 0;
}
