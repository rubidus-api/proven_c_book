/* Breaking the type, reading --- read a uint32_t, which needs 4-byte alignment, at an odd address. */
#include <stdint.h>

[[gnu::noinline]] static uint32_t read32(const unsigned char *p)
{
    return *(const uint32_t *)p;    /* the conversion itself is outside the contract (C23 6.3.2.3p7) */
}

int main(void)
{
    alignas(4) unsigned char b[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    return (int)(read32(b + 1) & 1);
}
