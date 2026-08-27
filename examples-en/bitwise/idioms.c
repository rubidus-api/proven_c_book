// The idioms one meets most often --- and why they work.
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

// Keep only the lowest set bit of x.
// On unsigned values 0u - x equals (~x + 1). Below the lowest set bit nothing
// changes and above it everything flips, so the AND leaves just that one bit.
static uint32_t lowest_one(uint32_t x) { return x & (0u - x); }

// Clear the lowest set bit. x-1 turns that bit into 0 and all below it into 1s.
static uint32_t clear_lowest_one(uint32_t x) { return x & (x - 1u); }

// Is it a power of two --- the same question as "is exactly one bit set".
static int is_power_of_two(uint32_t x) { return x != 0 && (x & (x - 1u)) == 0; }

// Round up to a multiple of a, where a is a power of two.
static uint32_t align_up(uint32_t x, uint32_t a) { return (x + a - 1u) & ~(a - 1u); }

// Rotate left by n. Safe when n == 0 as well --- see below.
static uint32_t rotate_left(uint32_t x, unsigned n)
{
    return (x << (n & 31)) | (x >> ((32u - n) & 31));
}

// How many 1 bits --- loops once per set bit (Kernighan's way).
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
