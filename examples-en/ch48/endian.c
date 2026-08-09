#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    uint32_t value = 0x12345678u;
    unsigned char bytes[4];

    memcpy(bytes, &value, sizeof value);   /* look into the representation byte by byte */

    printf("value: 0x%08X\n", value);
    printf("memory order: %02X %02X %02X %02X\n",
           bytes[0], bytes[1], bytes[2], bytes[3]);
    printf("this machine is %s-endian\n",
           bytes[0] == 0x78 ? "little" : "big");

    struct padded {
        char  tag;      /* 1 byte */
        int   count;    /* 4 bytes — alignment opens a gap in front of it */
    };
    printf("1 + 4 = 5, but sizeof = %zu\n", sizeof(struct padded));
    return 0;
}
