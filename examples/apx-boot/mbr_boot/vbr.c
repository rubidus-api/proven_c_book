/* VBR 의 본체. MBR 이 넘겨준 것을 확인하고, 디스크의 0번 섹터를 다시 읽어
   파티션 도구가 적은 표를 바이트 그대로 보인다. */
#include "bios.h"

static void say(const char *s) { while (*s) con_putc(*s++); }

static void dec(uint32_t v)
{
    char b[11], *p = b + 10;
    *p = '\0';
    do *--p = (char)('0' + v % 10); while (v /= 10);
    say(p);
}

static void hex(uint32_t v, int digits)
{
    while (digits--) con_putc("0123456789abcdef"[v >> 4 * digits & 15]);
}

static uint8_t sector0[512];

[[noreturn]] void vbr_main(uint8_t drive, const struct part *entry)
{
    uint16_t si = real_addr(entry);
    say("VBR: running at 0000:7c00, DL = "); hex(drive, 2);
    say(", SI = "); hex(si, 4);
    say(" (entry "); con_putc((char)('1' + (si - 0x07be) / 16)); say(" of the table at 0600)\r\n");
    say("VBR: my partition starts at LBA "); dec(entry->lba);
    say(", "); dec(entry->count); say(" sectors\r\n");

    if (disk_read(drive, 0, sector0) != 0) { say("VBR: read failed\r\n"); stop(1); }
// [excerpt table]
    say("VBR: LBA 0 again --- disk signature ");
    for (int i = 3; i >= 0; i--) hex(sector0[0x1b8 + i], 2);   /* 작은 끝 차례 */
    say(", then the table from 01be\r\n");
    for (int i = 0; i < 64; i++) {
        hex(sector0[0x1be + i], 2);
        say(i % 16 == 15 ? "\r\n" : " ");
    }
    const struct part *t = (const struct part *)(sector0 + 0x1be);
    for (int i = 0; i < 4; i++) {
        if (t[i].type == 0) continue;           /* 종류 0 = 빈 칸 */
        say(t[i].boot == 0x80 ? "  * " : "    ");
        con_putc((char)('1' + i));
        say("  type "); hex(t[i].type, 2);
        say("  start "); dec(t[i].lba);
        say("  sectors "); dec(t[i].count); say("\r\n");
    }
    say("VBR: last two bytes ");
    hex(sector0[510], 2); con_putc(' '); hex(sector0[511], 2); say("\r\n");
// [/excerpt]
    stop(0);
}
