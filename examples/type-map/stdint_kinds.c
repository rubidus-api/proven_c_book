/* <stdint.h> 의 세 갈래와 uint8_t 의 함정. */
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    puts("[three families - they demand different things]");
    printf("  %-16s %-6s %s\n", "type", "size", "what it guarantees");
    printf("  %-16s %-6zu %s\n", "uint8_t",       sizeof(uint8_t),
           "exactly 8 bits, no padding (optional)");
    printf("  %-16s %-6zu %s\n", "uint_least8_t", sizeof(uint_least8_t),
           "smallest type with at least 8 bits (required)");
    printf("  %-16s %-6zu %s\n", "uint_fast8_t",  sizeof(uint_fast8_t),
           "usually the fastest with at least 8 bits (required)");
    printf("  %-16s %-6zu %s\n", "uint_least32_t", sizeof(uint_least32_t), "at least 32 bits");
    printf("  %-16s %-6zu %s\n", "uint_fast32_t",  sizeof(uint_fast32_t),  "at least 32 bits, speed first");
    printf("  %-16s %-6zu %s\n", "uintmax_t",      sizeof(uintmax_t),      "the widest integer (required)");
    printf("  %-16s %-6zu %s\n", "uintptr_t",      sizeof(uintptr_t),      "round-trips a void* (optional)");

    puts("\n[trap 1: uint8_t is usually unsigned char, so it leaks out as a character]");
    uint8_t age = 65;
    printf("  as %%u: %u\n", age);
    printf("  as %%c: %c   <- you get 'A', not 65\n", age);
    printf("  putchar(age) is the same trap\n");

    puts("\n[trap 2: arithmetic promotes to int - this is not 8-bit arithmetic]");
    uint8_t a = 200, b = 100;
    printf("  a + b        = %d   <- computed as int, so 300 (it did not wrap)\n", a + b);
    printf("  (uint8_t)(a+b) = %u   <- it wraps only when stored back\n", (uint8_t)(a + b));
    printf("  sizeof(a + b) = %zu  <- the result is 4 bytes\n", sizeof(a + b));

    puts("\n[trap 3: they are only aliases, so the format macros must follow]");
    printf("  with PRIu8: %" PRIu8 "\n", age);
    printf("  UINT8_C(200) is %u, UINT8_MAX is %u\n", UINT8_C(200), UINT8_MAX);

    puts("\n[which one, when]");
    puts("  protocol, file format, register -> exact width (uint32_t)");
    puts("  portability first, width at least -> minimum width (uint_least16_t)");
    puts("  loop counter, local arithmetic    -> fastest width (uint_fast32_t)");
    puts("  size, index, byte count           -> size_t (<stddef.h>)");
    return 0;
}
