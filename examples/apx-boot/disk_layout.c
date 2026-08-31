/* 디스크의 첫 섹터에 무엇이 있는가 --- 직접 만들어 보고, 다시 읽어 본다.

   주의: 이 프로그램은 디스크를 건드리지 않는다. 파일도 장치도 열지 않고, 아무것도 쓰지
   않는다. 기억 속의 배열 하나를 옛 규약 그대로 채운 뒤, 그 배열을 다시 읽어 해독해
   화면에 찍을 뿐이다. 돌리면 글자만 나오고 끝난다.

   여기서 만드는 바이트는 진짜 규칙을 따른다: 512바이트, 0x1BE 의 파티션 표 네 칸,
   그리고 끝의 0x55 0xAA. GPT 를 쓰는 디스크의 첫 섹터(보호 MBR)와 EFI PART 헤더도
   같이 만든다. 이 바이트를 실제 디스크의 0번 섹터에 쓰는 일은 하지 않는다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint32_t crc32(const void *buf, size_t n)
{
    const unsigned char *p = buf;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1));
    }
    return c ^ 0xFFFFFFFFu;
}

static void put32(unsigned char *p, uint32_t v)
{ p[0] = v & 0xff; p[1] = v >> 8 & 0xff; p[2] = v >> 16 & 0xff; p[3] = v >> 24; }

static void put_part(unsigned char *e, uint8_t boot, uint8_t type,
                     uint32_t lba, uint32_t count)
{
    e[0] = boot;              /* 0x80 이면 「이것으로 부팅」 */
    e[1] = 0xfe; e[2] = 0xff; e[3] = 0xff;   /* 옛 CHS 자리 --- 이제는 채우는 시늉만 한다 */
    e[4] = type;
    e[5] = 0xfe; e[6] = 0xff; e[7] = 0xff;
    put32(e + 8, lba);        /* 시작 --- 이쪽이 진짜로 쓰인다 */
    put32(e + 12, count);
}

static void hexdump(const unsigned char *p, size_t off, size_t n)
{
    for (size_t i = 0; i < n; i += 16) {
        printf("  %04zx  ", off + i);
        for (size_t j = 0; j < 16; j++) printf("%02x%s", p[off + i + j], j == 7 ? "  " : " ");
        printf("\n");
    }
}

int main(void)
{
    unsigned char mbr[512] = { 0 };

    /* (1) 부트스트랩 자리 --- 446바이트. 진짜 부트로더의 첫 조각이 여기 들어간다. */
    memcpy(mbr, "\xfa\x31\xc0\x8e\xd8\x8e\xd0\xbc\x00\x7c", 10);  /* cli; 세그먼트·스택 차리기 */
    printf("첫 섹터의 자리 나눔\n");
    printf("  0x000 ~ 0x1bd : 부트스트랩 코드 %d바이트\n", 0x1be);
    printf("  0x1be ~ 0x1fd : 파티션 표 --- 16바이트 × 4칸\n");
    printf("  0x1fe ~ 0x1ff : 서명 0x55 0xaa\n\n");

    /* (2) 파티션 표 --- 흔한 배치 하나를 적어 둔다 */
    put_part(mbr + 0x1be, 0x80, 0x83, 2048, 1024000);   /* 부팅 가능, 리눅스 */
    put_part(mbr + 0x1ce, 0x00, 0x82, 1026048, 262144); /* 리눅스 스왑 */
    mbr[510] = 0x55; mbr[511] = 0xaa;

    printf("파티션 표 (0x1be 부터)\n");
    hexdump(mbr, 0x1be, 32);
    printf("\n  1번: 부팅 %s · 종류 0x%02x(리눅스) · 시작 LBA %u · %u섹터(%u MiB)\n",
           mbr[0x1be] == 0x80 ? "가능" : "아님", mbr[0x1be + 4],
           2048u, 1024000u, 1024000u / 2048u);
    printf("  2번: 부팅 %s · 종류 0x%02x(스왑) · 시작 LBA %u\n\n",
           mbr[0x1ce] == 0x80 ? "가능" : "아님", mbr[0x1ce + 4], 1026048u);

    printf("끝의 두 바이트: %02x %02x --- %s\n\n", mbr[510], mbr[511],
           (mbr[510] == 0x55 && mbr[511] == 0xaa)
           ? "BIOS 는 이것이 있어야 「부팅 섹터」로 인정한다" : "부팅 섹터가 아니다");

    /* (3) GPT 를 쓰는 디스크의 첫 섹터 --- 보호 MBR */
    unsigned char pmbr[512] = { 0 };
    put_part(pmbr + 0x1be, 0x00, 0xee, 1, 0xffffffffu);  /* 0xEE = 「여기부터 끝까지 남의 것」 */
    pmbr[510] = 0x55; pmbr[511] = 0xaa;
    printf("보호 MBR (GPT 디스크의 첫 섹터)\n");
    printf("  파티션 종류 0x%02x --- 옛 도구에게 「이 디스크는 전부 쓰였다」고 말한다\n",
           pmbr[0x1be + 4]);
    printf("  까닭: GPT 를 모르는 도구가 빈 디스크로 알고 덮어쓰는 것을 막으려고\n\n");

    /* (4) 진짜 지도는 두 번째 섹터(LBA 1)에 있다 --- GPT 헤더 */
    unsigned char gpt[92] = { 0 };
    memcpy(gpt, "EFI PART", 8);
    put32(gpt + 8, 0x00010000u);      /* 개정 1.0 */
    put32(gpt + 12, 92);              /* 헤더 크기 */
    put32(gpt + 16, 0);               /* CRC 자리는 0 으로 두고 계산한다 */
    put32(gpt + 16, crc32(gpt, 92));

    printf("GPT 헤더 (LBA 1)\n");
    printf("  서명   : %.8s\n", gpt);
    printf("  헤더 CRC32: 0x%08x --- 자기 자신을 검사한다(계산할 때 이 자리는 0)\n",
           (unsigned)(gpt[16] | gpt[17] << 8 | gpt[18] << 16 | (uint32_t)gpt[19] << 24));
    printf("  그리고 같은 표가 디스크의 *맨 끝*에 한 벌 더 있다 --- 하나가 깨져도 살아남게\n");
    return 0;
}
