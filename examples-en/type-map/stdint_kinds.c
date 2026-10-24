/* The three families of <stdint.h>, and the traps of uint8_t. */
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    puts("[three families — they demand different things]");
    printf("  %-16s %-6s %s\n", "type", "size", "what it guarantees");
    printf("  %-16s %-6zu %s\n", "uint8_t",       sizeof(uint8_t),
           "exactly 8 bits, no padding (optional)");
    printf("  %-16s %-6zu %s\n", "uint_least8_t", sizeof(uint_least8_t),
           "smallest with at least 8 bits (required)");
    printf("  %-16s %-6zu %s\n", "uint_fast8_t",  sizeof(uint_fast8_t),
           "usually fastest with at least 8 bits (required)");
    printf("  %-16s %-6zu %s\n", "uint_least32_t", sizeof(uint_least32_t), "at least 32 bits");
    printf("  %-16s %-6zu %s\n", "uint_fast32_t",  sizeof(uint_fast32_t),  "at least 32 bits, speed first");
    printf("  %-16s %-6zu %s\n", "uintmax_t",      sizeof(uintmax_t),      "the widest integer (required)");
    printf("  %-16s %-6zu %s\n", "uintptr_t",      sizeof(uintptr_t),      "void* round trip (optional)");

    puts("\n[trap 1: uint8_t is usually unsigned char, so it leaks as a character]");
    uint8_t age = 65;
    printf("  with %%u: %u\n", age);
    printf("  with %%c: %c   <- 'A' comes out, not 65\n", age);
    printf("  putchar(age) is the same trap\n");

    puts("\n[trap 2: arithmetic promotes to int — this is not 8-bit arithmetic]");
    uint8_t a = 200, b = 100;
    printf("  a + b        = %d   <- computed as int: 300, it did not wrap\n", a + b);
    printf("  (uint8_t)(a+b) = %u   <- put it back to make it wrap\n", (uint8_t)(a + b));
    printf("  sizeof(a + b) = %zu  <- the result is 4 bytes\n", sizeof(a + b));

    puts("\n[trap 3: being an alias, the format must follow the alias]");
    printf("  with PRIu8: %" PRIu8 "\n", age);
    printf("  UINT8_C(200) is %u, UINT8_MAX is %u\n", UINT8_C(200), UINT8_MAX);

    puts("\n[which to use when]");
    puts("  protocols, file formats, registers -> exact width (uint32_t)");
    puts("  portability first, width at least -> minimum width (uint_least16_t)");
    puts("  loop counters, local sums          -> fastest (uint_fast32_t)");
    puts("  sizes, indices, byte counts        -> size_t (<stddef.h>)");
    return 0;
}
