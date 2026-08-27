// The place bit work burns people most often --- integer promotion.
// Anything narrower than int is widened to int before the operation, so what
// you thought was "eight bits flipped" comes out thirty-two bits wide.
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int main(void)
{
    uint8_t c = 0x0F;

    // The type of ~c is int, not uint8_t.
    printf("c             = 0x%02X\n", c);
    printf("~c            = 0x%08X   <- not eight bits\n", (unsigned)~c);
    printf("(uint8_t)~c   = 0x%02X         <- narrow it back yourself\n",
           (unsigned)(uint8_t)~c);
    // Writing (~c == 0xF0) here is stopped by gcc with -Wsign-compare.
    printf("((uint8_t)~c == 0xF0) is %s\n", ((uint8_t)~c == 0xF0) ? "true" : "false");

    // Where char is signed, a byte of 0x80 or more becomes negative when widened,
    // which is why bytes are held in unsigned char.
    char signed_byte = (char)0x80;
    unsigned char plain_byte = 0x80;
    printf("a char holding 0x80, widened  : %d\n", (int)signed_byte);
    printf("an unsigned char holding 0x80 : %d\n", (int)plain_byte);
    printf("masking with 0xFF fixes it    : %d\n", (int)(signed_byte & 0xFF));

    // A mask has a width too --- the width of its type.
    uint64_t wide = 0xFFFF'FFFF'FFFF'FFFFu;
    printf("wide & ~0u          = 0x%016" PRIX64 "   <- ~0u is 32 bits wide\n",
           wide & ~0u);
    printf("wide & ~UINT64_C(0) = 0x%016" PRIX64 "\n", wide & ~UINT64_C(0));
    return 0;
}
