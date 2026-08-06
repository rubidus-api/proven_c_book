#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    uint32_t value = 0x12345678u;
    unsigned char bytes[4];

    memcpy(bytes, &value, sizeof value);   /* 표현을 바이트로 들여다본다 */

    printf("value: 0x%08X\n", value);
    printf("memory order: %02X %02X %02X %02X\n",
           bytes[0], bytes[1], bytes[2], bytes[3]);
    printf("this machine is %s-endian\n",
           bytes[0] == 0x78 ? "little" : "big");

    struct padded {
        char  tag;      /* 1바이트 */
        int   count;    /* 4바이트 — 정렬 때문에 앞에 빈틈이 생긴다 */
    };
    printf("1 + 4 = 5, but sizeof = %zu\n", sizeof(struct padded));
    return 0;
}
