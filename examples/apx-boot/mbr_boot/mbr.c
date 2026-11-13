/* MBR 의 부트 코드. 440바이트 안에 들어가야 한다(0x1b8 부터 디스크 서명 4바이트, 0x1be 부터 표).
   하는 일은 부팅 사슬의 한 칸 그대로다: 다음 것을 찾고, 읽어 싣고, 맞는지 보고, 넘긴다. */
#include "bios.h"

static void say(const char *s) { while (*s) con_putc(*s++); }

// [excerpt mbr]
void mbr_main(uint8_t drive, const struct part *table)
{
    /* 찾는다: 0x80 이 붙은 첫 칸 */
    const struct part *active = nullptr;
    for (int i = 0; i < 4; i++)
        if (table[i].boot == 0x80) { active = &table[i]; break; }
    if (!active) { say("MBR: no active partition\r\n"); stop(1); }

    /* 싣는다: 그 파티션의 첫 섹터를 0x7c00 에 */
    uint8_t *vbr = load_area();
    if (disk_read(drive, active->lba, vbr) != 0)
        { say("MBR: read failed\r\n"); stop(1); }

    /* 본다: BIOS 가 한 검사를 한 번 더 */
    if (vbr[510] != 0x55 || vbr[511] != 0xaa) { say("MBR: no 55 aa\r\n"); stop(1); }

    /* 넘긴다: DL = 드라이브, SI = 표의 그 칸 */
    chain(drive, active, vbr);
}
// [/excerpt]
