/* 부트 코드가 기계에 닿는 자리는 이 머리말의 함수뿐이다.
   QEMU 의 PC 기계에서는 bios_int.c 가 BIOS 의 int 13h 와 직렬 포트를 진짜로 부르고,
   보통 PC 에서는 bios_trace.c 가 받은 명령을 찍는다. MBR 과 VBR 의 코드는 한 벌이다. */
#ifndef BIOS_H
#define BIOS_H
#include <stdint.h>

/* 파티션 표의 한 칸, 16바이트. 표는 0x1be 에서 시작해 4바이트 경계에 놓이지 않으므로,
   이 구조체를 아무 바이트 자리에나 겹쳐 읽을 수 있게 정렬 요구를 1로 낮춘다(GCC 의 packed).
   x86 은 어긋난 읽기를 하드웨어가 받아 주지만, C 는 그것을 약속하지 않는다. */
struct [[gnu::packed]] part {
    uint8_t  boot;           /* 0x80 이면 「여기로 부팅」 */
    uint8_t  chs_first[3];   /* 옛 CHS 주소 --- 요즘 도구는 채우는 시늉만 한다 */
    uint8_t  type;           /* 0x83 리눅스, 0x82 스왑, 0xee 「GPT 가 덮은 디스크」 */
    uint8_t  chs_last[3];
    uint32_t lba;            /* 시작 섹터 --- 이쪽이 진짜로 쓰인다 */
    uint32_t count;          /* 섹터 수 */
};
static_assert(sizeof(struct part) == 16);

void     con_putc(char c);                                   /* 직렬 포트로 한 글자 */
int      disk_read(uint8_t drive, uint32_t lba, void *dst);  /* 섹터 하나, 0 이면 성공 */
uint8_t *load_area(void);                                    /* 다음 단계를 싣는 자리, 0x7c00 */
uint16_t real_addr(const void *p);                           /* 실모드에서 쓰는 주소 */
[[noreturn]] void chain(uint8_t drive, const struct part *entry, void *code);
[[noreturn]] void stop(uint8_t code);                        /* 0 이면 끝까지 갔다 */

#endif
