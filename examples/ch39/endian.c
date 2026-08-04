#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    uint32_t value = 0x12345678u;
    unsigned char bytes[4];

    memcpy(bytes, &value, sizeof value);   /* 표현을 바이트로 들여다본다 */

    printf("값: 0x%08X\n", value);
    printf("메모리 순서: %02X %02X %02X %02X\n",
           bytes[0], bytes[1], bytes[2], bytes[3]);
    printf("이 기계는 %s 엔디안이다\n",
           bytes[0] == 0x78 ? "리틀" : "빅");

    struct padded {
        char  tag;      /* 1바이트 */
        int   count;    /* 4바이트 — 정렬 때문에 앞에 빈틈이 생긴다 */
    };
    printf("1 + 4 = 5 이지만, sizeof = %zu\n", sizeof(struct padded));
    return 0;
}
