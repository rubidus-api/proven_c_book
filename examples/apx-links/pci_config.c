/* PCI 설정 공간(256바이트)을 필드대로 짓고 되읽는다. 그리고 BAR 의 *크기를 알아내는*
   고전적인 수법 --- 전부 1을 써 보고 되읽기 --- 을 장치 흉내로 재현한다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static unsigned char cfg[256];
/* 각 BAR 가 실제로 요구하는 크기. 장치가 하드웨어로 정해 두는 값이다. */
static uint32_t bar_size[6] = { 16u << 20, 0, 256u << 20, 0, 0, 256 };
static int bar_is_io[6]     = { 0, 0, 0, 0, 0, 1 };
static int bar_is_64[6]     = { 0, 0, 1, 0, 0, 0 };   /* BAR2+3 이 짝을 이룬다 */

static uint32_t rd32(unsigned off)
{ uint32_t v = 0; for (int i = 3; i >= 0; i--) v = v << 8 | cfg[off + i]; return v; }
static void wr32(unsigned off, uint32_t v)
{ for (int i = 0; i < 4; i++) cfg[off + i] = (unsigned char)(v >> (8 * i)); }
static uint16_t rd16(unsigned off) { return (uint16_t)(cfg[off] | cfg[off + 1] << 8); }

/* 장치 흉내: BAR 에 값을 쓰면, 크기보다 낮은 비트는 장치가 *0 으로 굳혀* 돌려준다. */
static void bar_write(int i, uint32_t v)
{
    unsigned off = 0x10 + 4u * (unsigned)i;
    uint32_t low = bar_is_io[i] ? 1u : (uint32_t)(bar_is_64[i] ? 0x4 : 0x0);
    uint32_t mask = ~(bar_size[i] - 1);
    wr32(off, (v & mask) | low);
}

int main(void)
{
    /* ── 머리말 필드 ── */
    wr32(0x00, 0x10FA8086u);           /* 0x00 제조사 0x8086, 0x02 장치 0x10FA */
    wr32(0x04, 0x00100007u);           /* 0x04 명령: 메모리·IO·버스마스터 켬 / 0x06 상태 */
    wr32(0x08, 0x02000003u);           /* 0x08 개정 03, 0x09~0x0B 분류: 02 00 00 = 이더넷 */
    wr32(0x0C, 0x00000010u);           /* 0x0C 캐시줄 16, 0x0E 머리말 종류 0 */
    for (int i = 0; i < 6; i++)
        if (bar_size[i])
            /* 입출력 공간은 64 KiB 짜리 좁은 세계라 주소도 작다(옛 x86 의 흔적) */
            bar_write(i, bar_is_io[i] ? 0xC000u : 0xF0000000u + (uint32_t)i * 0x1000000u);
    wr32(0x2C, 0x00108086u);           /* 하위 시스템 */
    cfg[0x34] = 0x50;                  /* 능력 목록의 첫 자리 */
    cfg[0x3C] = 11;                    /* 인터럽트 선 (옛 방식) */
    cfg[0x3D] = 1;                     /* 인터럽트 핀 A */
    /* 능력 목록: MSI(0x05) → PCIe(0x10) → 끝 */
    cfg[0x50] = 0x05; cfg[0x51] = 0x60;
    cfg[0x60] = 0x10; cfg[0x61] = 0x00;

    printf("== configuration space header (type 0) ==\n");
    printf("  %-8s %-6s %-18s %s\n", "offset", "size", "name", "value");
    printf("  0x00     2      vendor ID          0x%04X%s\n", rd16(0x00),
           rd16(0x00) == 0x8086 ? " (Intel)" : "");
    printf("  0x02     2      device ID          0x%04X\n", rd16(0x02));
    printf("  0x04     2      command            0x%04X  [IO %s · memory %s · bus master %s]\n",
           rd16(0x04), rd16(0x04) & 1 ? "on" : "off", rd16(0x04) & 2 ? "on" : "off",
           rd16(0x04) & 4 ? "on" : "off");
    printf("  0x06     2      status             0x%04X  [capability list %s]\n", rd16(0x06),
           rd16(0x06) & 0x10 ? "present" : "absent");
    printf("  0x08     1      revision           0x%02X\n", cfg[0x08]);
    printf("  0x09-0B  3      class              %02X %02X %02X = %s\n",
           cfg[0x0B], cfg[0x0A], cfg[0x09],
           cfg[0x0B] == 0x02 ? "network controller (Ethernet)" : "other");
    printf("  0x0E     1      header type        0x%02X  (0=device, 1=bridge)\n", cfg[0x0E]);
    printf("  0x34     1      capability pointer 0x%02X\n", cfg[0x34]);
    printf("  0x3C-3D  2      interrupt line/pin %u / %c\n\n", cfg[0x3C], 'A' + cfg[0x3D] - 1);

    printf("== decoding the BARs ==\n");
    printf("  %-6s %-14s %-12s %-10s %s\n", "BAR", "raw value", "kind", "note", "base address");
    for (int i = 0; i < 6; i++) {
        uint32_t v = rd32(0x10 + 4u * (unsigned)i);
        if (v == 0) { printf("  BAR%-3d (unused)\n", i); continue; }
        if (v & 1) {
            printf("  BAR%-3d 0x%08X     %-12s %-10s 0x%08X\n", i, v, "I/O space", "---",
                   v & ~0x3u);
        } else {
            const char *w = ((v >> 1) & 3) == 2 ? "64-bit" : "32-bit";
            printf("  BAR%-3d 0x%08X     %-12s %-10s 0x%08X\n", i, v, "memory space",
                   w, v & ~0xFu);
            if (((v >> 1) & 3) == 2) { printf("  BAR%-3d (upper 32 bits of the BAR above)\n", i + 1); i++; }
        }
    }

    printf("\n== how the size is found --- write all ones and read back ==\n");
    printf("  a device holds the bits below its requested size at zero.\n");
    printf("  so write all ones and the zeros read back tell you the size.\n\n");
    for (int i = 0; i < 6; i++) {
        if (!bar_size[i]) continue;
        unsigned off = 0x10 + 4u * (unsigned)i;
        uint32_t saved = rd32(off);
        bar_write(i, 0xFFFFFFFFu);                 /* ① 전부 1을 쓴다 */
        uint32_t probe = rd32(off);                /* ② 되읽는다 */
        uint32_t mask = probe & (bar_is_io[i] ? ~0x3u : ~0xFu);
        uint32_t size = ~mask + 1;                 /* ③ 뒤집고 1을 더하면 크기 */
        bar_write(i, saved);                       /* ④ 원래 값을 되돌려 놓는다 */
        printf("  BAR%d: read back 0x%08X -> size %u bytes (%s)  %s\n", i, probe, size,
               size >= (1u << 20) ? "in MiB" : size >= 1024 ? "in KiB" : "bytes",
               size == bar_size[i] ? "matches what the device asked for" : "does not match");
    }
    printf("\n  * these four steps are what real firmware and kernels do. Forget the last one\n");
    printf("    (restoring) and the device lands at the wrong address and the machine hangs.\n");

    printf("\n== walking the capability list ==\n");
    unsigned p = cfg[0x34];
    while (p) {
        unsigned id = cfg[p], next = cfg[p + 1];
        printf("  0x%02X: capability 0x%02X (%s) -> next 0x%02X\n", p, id,
               id == 0x05 ? "MSI --- an interrupt sent as a memory write" :
               id == 0x10 ? "PCI Express" : "other", next);
        p = next;
    }
    printf("  * it is a linked list. Zero means the end --- following the chain tells you\n");
    printf("    what this device knows how to do.\n");
    return 0;
}
