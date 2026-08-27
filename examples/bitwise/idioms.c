// 자주 쓰는 비트 관용구들 --- 그리고 왜 그렇게 되는지.
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

// x 에서 가장 낮은 1 비트만 남긴다.
// 무부호에서 0u - x 는 (~x + 1) 과 같다. 가장 낮은 1 비트 *아래*는 그대로이고
// 그 위는 전부 뒤집히므로, 둘을 AND 하면 그 한 비트만 살아남는다.
static uint32_t lowest_one(uint32_t x) { return x & (0u - x); }

// 가장 낮은 1 비트를 지운다. x-1 은 그 비트를 0 으로 만들고 아래를 전부 1 로 만든다.
static uint32_t clear_lowest_one(uint32_t x) { return x & (x - 1u); }

// 2 의 거듭제곱인가 --- 1 비트가 정확히 하나인가와 같은 말이다.
static int is_power_of_two(uint32_t x) { return x != 0 && (x & (x - 1u)) == 0; }

// a(2의 거듭제곱)의 배수로 올림한다.
static uint32_t align_up(uint32_t x, uint32_t a) { return (x + a - 1u) & ~(a - 1u); }

// 왼쪽으로 n 만큼 회전. n == 0 일 때도 안전하다 --- 아래를 보라.
static uint32_t rotate_left(uint32_t x, unsigned n)
{
    return (x << (n & 31)) | (x >> ((32u - n) & 31));
}

// 1 비트의 개수 --- 켜진 비트 수만큼만 돈다(커니핸 방식).
static unsigned count_ones(uint32_t x)
{
    unsigned n = 0;
    while (x) { x &= x - 1u; n++; }
    return n;
}

int main(void)
{
    uint32_t x = 0x0000'0B40;          // …1011 0100 0000
    printf("x = 0x%08" PRIX32 "\n", x);
    printf("  lowest set bit  (x & -x)     = 0x%08" PRIX32 "\n", lowest_one(x));
    printf("  clear lowest    (x & (x-1))  = 0x%08" PRIX32 "\n", clear_lowest_one(x));
    printf("  number of 1 bits             = %u\n", count_ones(x));

    puts("powers of two:");
    for (uint32_t v = 0; v <= 5; v++)
        printf("  %" PRIu32 " -> %s\n", v, is_power_of_two(v) ? "yes" : "no");
    printf("  1024 -> %s, 1000 -> %s\n",
           is_power_of_two(1024) ? "yes" : "no", is_power_of_two(1000) ? "yes" : "no");

    puts("rounding up to a multiple of 16:");
    for (uint32_t v = 0; v <= 33; v += 16)
        printf("  align_up(%2" PRIu32 ", 16) = %" PRIu32 "\n", v, align_up(v, 16));
    printf("  align_up(17, 16) = %" PRIu32 "\n", align_up(17, 16));

    puts("rotation keeps every bit --- nothing falls off the end:");
    printf("  rotate_left(0x80000001, 1) = 0x%08" PRIX32 "\n", rotate_left(0x8000'0001u, 1));
    printf("  rotate_left(0x80000001, 0) = 0x%08" PRIX32 "  (n = 0 is safe here)\n",
           rotate_left(0x8000'0001u, 0));
    return 0;
}
