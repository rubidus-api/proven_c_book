// The five basic moves for working with bits.
// All of them on unsigned types, with the width written into the type name.
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

static void show(const char *label, uint32_t v)
{
    printf("  %-28s 0x%08" PRIX32 "\n", label, v);
}

int main(void)
{
    uint32_t v = 0x00F0'0000;          // C23's digit separator
    show("start", v);

    // (1) set --- put a 1 in that place
    v |= UINT32_C(1) << 3;
    show("set bit 3        (|=)", v);

    // (2) clear --- AND with a mask that is 0 only there
    v &= ~(UINT32_C(1) << 20);
    show("clear bit 20     (&= ~)", v);

    // (3) test --- the result is 0 or *the value of that bit*, not 1
    printf("  is bit 3 on?  %s\n", (v & (UINT32_C(1) << 3)) ? "yes" : "no");
    printf("  the value of v & (1<<3) is %" PRIu32 ", not 1\n",
           v & (UINT32_C(1) << 3));

    // (4) flip --- XOR inverts exactly where the mask has 1s
    v ^= UINT32_C(0xF0);
    show("flip bits 4..7   (^=)", v);

    // (5) replace a field --- clear it, then shift the new value in
    // Treat bits 8..15 as one eight-bit field.
    uint32_t field = 0xAB;
    uint32_t mask  = UINT32_C(0xFF) << 8;
    v = (v & ~mask) | ((field << 8) & mask);
    show("put 0xAB into bits 8..15", v);

    // Reading it back is the reverse --- shift down, then mask
    printf("  reading it back gives 0x%02" PRIX32 "\n", (v >> 8) & 0xFF);
    return 0;
}
