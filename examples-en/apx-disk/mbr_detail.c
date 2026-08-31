/* MBR 을 필드 단위로 짓고 되읽는다 --- 표에 적은 오프셋이 진짜인지 코드가 증언한다.
   주의: 디스크를 건드리지 않는다. 기억 속 배열만 채우고 다시 읽어 화면에 찍는다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SEC 512u

static void put32(unsigned char *p, uint32_t v)
{ p[0] = v & 0xff; p[1] = v >> 8 & 0xff; p[2] = v >> 16 & 0xff; p[3] = v >> 24; }
static uint32_t get32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* CHS 는 3바이트에 실린다: 머리 8비트, 실린더 10비트, 섹터 6비트.
   실린더의 위 두 비트가 섹터 바이트의 위 두 비트로 올라간다 --- 그 비좁음이 한계를 만든다. */
static void put_chs(unsigned char *p, uint32_t cyl, uint32_t head, uint32_t sect)
{
    if (cyl > 1023) { p[0] = 0xfe; p[1] = 0xff; p[2] = 0xff; return; }  /* 못 적으면 관용값 */
    p[0] = (unsigned char)head;
    p[1] = (unsigned char)(((cyl >> 2) & 0xc0) | (sect & 0x3f));
    p[2] = (unsigned char)(cyl & 0xff);
}
static void get_chs(const unsigned char *p, unsigned *cyl, unsigned *head, unsigned *sect)
{ *head = p[0]; *sect = p[1] & 0x3f; *cyl = ((unsigned)(p[1] & 0xc0) << 2) | p[2]; }

static void put_part(unsigned char *e, uint8_t boot, uint8_t type,
                     uint32_t lba, uint32_t count)
{
    e[0] = boot;
    put_chs(e + 1, lba / (255 * 63), (lba / 63) % 255, lba % 63 + 1);
    e[4] = type;
    put_chs(e + 5, (lba + count - 1) / (255 * 63), ((lba + count - 1) / 63) % 255,
            (lba + count - 1) % 63 + 1);
    put32(e + 8, lba);
    put32(e + 12, count);
}

static const char *type_name(uint8_t t)
{
    switch (t) {
    case 0x00: return "empty";
    case 0x05: return "extended (CHS)";
    case 0x07: return "NTFS / exFAT";
    case 0x0b: return "FAT32 (CHS)";
    case 0x0c: return "FAT32 (LBA)";
    case 0x0e: return "FAT16 (LBA)";
    case 0x0f: return "extended (LBA)";
    case 0x82: return "Linux swap";
    case 0x83: return "Linux";
    case 0xee: return "GPT protective MBR";
    case 0xef: return "EFI system partition";
    default:   return "other";
    }
}

static void show_entry(const char *tag, const unsigned char *e, uint32_t base)
{
    unsigned c0, h0, s0, c1, h1, s1;
    get_chs(e + 1, &c0, &h0, &s0);
    get_chs(e + 5, &c1, &h1, &s1);
    uint32_t rel = get32(e + 8), cnt = get32(e + 12);
    if (e[4] == 0 && cnt == 0) { printf("  %-10s (empty)\n", tag); return; }
    printf("  %-10s bootable=%s type=0x%02x (%s)\n", tag, e[0] == 0x80 ? "yes" : "no",
           e[4], type_name(e[4]));
    printf("             start LBA=%u (relative) -> %u (absolute)  size=%u sectors = %.1f MiB\n",
           rel, base + rel, cnt, cnt * (double)SEC / (1024 * 1024));
    printf("             CHS start=(%u,%u,%u) end=(%u,%u,%u)%s\n",
           c0, h0, s0, c1, h1, s1,
           (e[1] == 0xfe && e[2] == 0xff) ? "  <- beyond what CHS can express (the idiomatic value)" : "");
}

int main(void)
{
    unsigned char mbr[SEC] = { 0 };

    /* ── 주 파티션 넷 중 셋 + 확장 하나 ─────────────────────────── */
    put_part(mbr + 0x1be, 0x80, 0x0c, 2048,     204800);      /* FAT32(LBA) 100 MiB, 부팅 */
    put_part(mbr + 0x1ce, 0x00, 0x83, 206848,   2097152);     /* 리눅스 1 GiB */
    put_part(mbr + 0x1de, 0x00, 0x82, 2304000,  262144);      /* 스왑 128 MiB */
    put_part(mbr + 0x1ee, 0x00, 0x0f, 2566144,  4194304);     /* 확장(LBA) 2 GiB */
    mbr[510] = 0x55; mbr[511] = 0xaa;

    printf("== the MBR partition table (LBA 0) ==\n");
    for (int i = 0; i < 4; i++) {
        char tag[16]; snprintf(tag, sizeof tag, "entry %d", i + 1);
        show_entry(tag, mbr + 0x1be + 16 * i, 0);
    }
    printf("  signature 0x%02x%02x --- %s\n\n", mbr[510], mbr[511],
           (mbr[510] == 0x55 && mbr[511] == 0xaa) ? "accepted as a boot sector" : "not accepted");

    /* ── 확장 파티션 안의 EBR 사슬 ──────────────────────────────
       규칙이 둘이고, 둘의 기준이 *다르다*:
         1번 항목 = 이 EBR 바로 뒤의 논리 파티션 → 이 EBR 기준의 상대 LBA
         2번 항목 = 다음 EBR 의 자리          → 확장 파티션 시작 기준의 상대 LBA
       이 어긋남이 EBR 을 손으로 읽을 때 가장 많이 틀리는 자리다. */
    const uint32_t ext_start = 2566144;
    unsigned char ebr1[SEC] = { 0 }, ebr2[SEC] = { 0 };

    put_part(ebr1 + 0x1be, 0x00, 0x83, 2048, 1048576);        /* 논리 1: 512 MiB */
    put_part(ebr1 + 0x1ce, 0x00, 0x0f, 1050624, 2097152);     /* 다음 EBR: 확장 시작 기준 */
    ebr1[510] = 0x55; ebr1[511] = 0xaa;

    put_part(ebr2 + 0x1be, 0x00, 0x07, 2048, 2095104);        /* 논리 2: NTFS */
    ebr2[510] = 0x55; ebr2[511] = 0xaa;                       /* 2번 항목 비었다 = 사슬 끝 */

    printf("== the EBR chain inside the extended partition (extended start LBA=%u) ==\n", ext_start);
    uint32_t ebr_lba = ext_start;
    const unsigned char *chain[] = { ebr1, ebr2 };
    for (int i = 0; i < 2; i++) {
        printf("\n  [EBR %d] absolute LBA of this EBR = %u\n", i + 1, ebr_lba);
        show_entry("logical", chain[i] + 0x1be, ebr_lba);        /* 기준: 이 EBR */
        show_entry("next EBR", chain[i] + 0x1ce, ext_start);   /* 기준: 확장 시작 */
        uint32_t next = get32(chain[i] + 0x1ce + 8);
        if (next == 0) { printf("  -> end of the chain\n"); break; }
        ebr_lba = ext_start + next;
    }

    /* ── CHS 의 한계 ─────────────────────────────────────────── */
    printf("\n== the limit of what CHS can express ==\n");
    unsigned long long chs_max = 1024ull * 255 * 63 * SEC;
    printf("  1024 cylinders x 255 heads x 63 sectors x %u bytes = %llu bytes = %.1f GB\n",
           SEC, chs_max, chs_max / 1e9);
    printf("  beyond that the LBA field (4 bytes) takes over: 2^32 sectors x %u = %.1f TB\n",
           SEC, 4294967296.0 * SEC / 1e12);
    printf("  -> the MBR 2 TiB limit comes from here (the sector number is 32 bits).\n");
    return 0;
}
