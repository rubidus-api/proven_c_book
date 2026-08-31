/* 파티션 *안*의 첫 섹터 --- FAT32 의 BPB 를 짓고 되읽어, 「클러스터 번호 → 섹터 번호」
   산수를 실제로 해 본다. 그리고 FAT 사슬을 하나 따라간다.
   주의: 디스크를 건드리지 않는다. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SEC 512u

static void put16(unsigned char *p, uint16_t v) { p[0] = v & 0xff; p[1] = v >> 8; }
static void put32(unsigned char *p, uint32_t v)
{ for (int i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i)); }
static uint16_t get16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t get32(const unsigned char *p)
{ uint32_t v = 0; for (int i = 3; i >= 0; i--) v = v << 8 | p[i]; return v; }

int main(void)
{
    unsigned char bs[SEC] = { 0 };
    const uint32_t part_lba = 2048;        /* 이 파티션이 디스크에서 시작하는 자리 */
    const uint32_t tot_sec  = 2097152;     /* 1 GiB */
    const uint32_t spc_want = 8;           /* 클러스터 = 8섹터 = 4 KiB */
    const uint32_t rsvd_want = 32, nfat_want = 2;

    /* FAT 한 벌의 크기는 마음대로 정하는 값이 아니라 *계산해서 나오는* 값이다.
       규격이 주는 어림 공식(FAT32): 자료가 될 섹터를 「클러스터 하나가 먹는 섹터 +
       그 클러스터를 가리키는 FAT 항목이 먹는 자리」로 나눈다. */
    uint32_t t1 = tot_sec - rsvd_want;
    uint32_t t2 = (256 * spc_want + nfat_want) / 2;
    const uint32_t fat_sz = (t1 + t2 - 1) / t2;

    /* ── BPB 를 규격대로 채운다 ─────────────────────────────── */
    bs[0] = 0xeb; bs[1] = 0x58; bs[2] = 0x90;      /* 0  : 점프 명령(3바이트) */
    memcpy(bs + 3, "MSWIN4.1", 8);                 /* 3  : 만든 곳 이름(8) */
    put16(bs + 11, 512);                           /* 11 : 섹터 하나의 바이트 수 */
    bs[13] = (unsigned char)spc_want;              /* 13 : 클러스터 하나의 섹터 수 */
    put16(bs + 14, (uint16_t)rsvd_want);           /* 14 : 예약 섹터 수(FAT 앞) */
    bs[16] = (unsigned char)nfat_want;             /* 16 : FAT 벌 수(대개 2 --- 사본) */
    put16(bs + 17, 0);                             /* 17 : 루트 항목 수 --- FAT32 는 0 */
    put16(bs + 19, 0);                             /* 19 : 총 섹터(16비트) --- 안 쓰면 0 */
    bs[21] = 0xf8;                                 /* 21 : 매체 종류(고정 디스크) */
    put16(bs + 22, 0);                             /* 22 : FAT 크기(16비트) --- FAT32 는 0 */
    put16(bs + 24, 63);                            /* 24 : 트랙당 섹터(옛 CHS 흔적) */
    put16(bs + 26, 255);                           /* 26 : 머리 수(옛 CHS 흔적) */
    put32(bs + 28, part_lba);                      /* 28 : 이 파티션 앞의 숨은 섹터 수 */
    put32(bs + 32, tot_sec);                       /* 32 : 총 섹터(32비트) */
    put32(bs + 36, fat_sz);                        /* 36 : FAT 한 벌의 섹터 수(계산값) */
    put16(bs + 40, 0);                             /* 40 : FAT 미러링 표시 */
    put16(bs + 42, 0);                             /* 42 : 파일 시스템 판 번호 */
    put32(bs + 44, 2);                             /* 44 : 루트 디렉터리의 클러스터 번호 */
    put16(bs + 48, 1);                             /* 48 : FSInfo 섹터 */
    put16(bs + 50, 6);                             /* 50 : 부트 섹터 사본의 자리 */
    bs[64] = 0x80;                                 /* 64 : BIOS 드라이브 번호 */
    bs[66] = 0x29;                                 /* 66 : 확장 서명 --- 아래 셋이 있다는 표시 */
    put32(bs + 67, 0x1234ABCDu);                   /* 67 : 볼륨 일련번호 */
    memcpy(bs + 71, "NO NAME    ", 11);            /* 71 : 볼륨 이름(11) */
    memcpy(bs + 82, "FAT32   ", 8);                /* 82 : 파일 시스템 이름표(믿지 말 것) */
    bs[510] = 0x55; bs[511] = 0xaa;                /* 510: 서명 */

    /* ── 되읽는다 ───────────────────────────────────────────── */
    uint32_t bps   = get16(bs + 11);
    uint32_t spc   = bs[13];
    uint32_t rsvd  = get16(bs + 14);
    uint32_t nfat  = bs[16];
    uint32_t fatsz = get32(bs + 36);
    uint32_t tot   = get32(bs + 32);
    uint32_t root  = get32(bs + 44);

    printf("== the FAT32 BPB (first sector of the partition) ==\n");
    printf("  sector size       : %u bytes\n", bps);
    printf("  cluster size      : %u sectors = %u bytes (%u KiB)\n", spc, spc * bps, spc * bps / 1024);
    printf("  reserved sectors  : %u\n", rsvd);
    printf("  FAT copies / size : %u x %u sectors\n", nfat, fatsz);
    printf("  total sectors     : %u = %.1f MiB\n", tot, tot * (double)bps / (1024 * 1024));
    printf("  root cluster      : %u\n", root);
    printf("  volume label      : \"%.11s\", filesystem label \"%.8s\"\n",
           bs + 71, bs + 82);
    printf("  hidden sectors    : %u  <- where this partition starts on the disk\n\n",
           get32(bs + 28));

    /* ── 자리 계산 ──────────────────────────────────────────── */
    uint32_t first_data = rsvd + nfat * fatsz;         /* FAT32 는 루트 디렉터리 자리가 따로 없다 */
    uint32_t clusters   = (tot - first_data) / spc;

    printf("== how the partition divides up ==\n");
    printf("  reserved area : sectors 0 - %u  (boot sector, FSInfo, copies)\n", rsvd - 1);
    printf("  FAT copy 1    : sectors %u - %u\n", rsvd, rsvd + fatsz - 1);
    printf("  FAT copy 2    : sectors %u - %u  <- a duplicate of the same content\n",
           rsvd + fatsz, rsvd + 2 * fatsz - 1);
    printf("  data area     : from sector %u  (cluster numbers start here)\n", first_data);
    printf("  clusters      : %u --- FAT32 needs at least 65525 -> %s\n\n",
           clusters, clusters >= 65525 ? "it is FAT32" : "cannot be FAT32");

    printf("== changing the cluster size on the same volume ==\n");
    /* 한글은 한 글자가 두 칸이라 %-10s 로는 안 맞는다 --- 머리글은 손으로 맞춘다 */
    printf("  cluster   clusters       one FAT      usable as FAT32\n");
    for (uint32_t sc = 1; sc <= 64; sc *= 2) {
        uint32_t a  = tot - rsvd;
        uint32_t b  = (256 * sc + nfat) / 2;
        uint32_t fz = (a + b - 1) / b;                 /* 규격의 어림 공식 */
        uint32_t cl = (tot - (rsvd + nfat * fz)) / sc; /* 실제로 남는 클러스터 수 */
        char csz[16];
        snprintf(csz, sizeof csz, "%u KiB", sc * bps / 1024 ? sc * bps / 1024 : 0);
        if (sc * bps < 1024) snprintf(csz, sizeof csz, "%u B", sc * bps);
        char fsz[16]; snprintf(fsz, sizeof fsz, "%u sectors", fz);
        printf("  %-10s %-14u %-12s %s\n", csz, cl, fsz,
               cl >= 65525 ? "yes" : "no --- too few clusters");
    }
    printf("\n  -> this is why a small volume ends up FAT16. Larger clusters mean fewer\n");
    printf("     of them to manage, and the count falls below 65525.\n\n");

    printf("== cluster number -> sector number ==\n");
    printf("  formula: start of data area + (N - 2) x sectors per cluster\n");
    printf("        (why subtract 2: 0 and 1 are used as labels and have no real place)\n\n");
    for (uint32_t n = 2; n <= 5; n++) {
        uint32_t rel = first_data + (n - 2) * spc;
        printf("  cluster %-3u -> sector %-6u in the partition -> absolute LBA %-8u%s\n",
               n, rel, part_lba + rel, n == root ? "  <- the root directory" : "");
    }

    /* ── FAT 사슬 따라가기 ──────────────────────────────────── */
    static unsigned char fat[SEC * 4];      /* FAT 앞 네 섹터만 흉내 낸다 */
    put32(fat + 3 * 4, 4);                  /* 3번 다음은 4번 */
    put32(fat + 4 * 4, 7);                  /* 4번 다음은 7번 */
    put32(fat + 7 * 4, 0x0FFFFFFFu);        /* 7번이 마지막 */

    printf("\n== when one file is scattered --- the FAT chain ==\n");
    printf("  the FAT is an array of cluster number -> next cluster number (4 bytes each).\n");
    printf("  if a file starts at cluster 3:\n\n");
    uint32_t n = 3;
    for (int step = 0; step < 8; step++) {
        uint32_t next = get32(fat + n * 4) & 0x0FFFFFFFu;   /* 위 4비트는 예약 */
        uint32_t lba  = part_lba + first_data + (n - 2) * spc;
        printf("    cluster %-3u (LBA %-8u, %u KiB)", n, lba, spc * bps / 1024);
        if (next >= 0x0FFFFFF8u) { printf(" -> end of chain\n"); break; }
        if (next == 0x0FFFFFF7u) { printf(" -> bad cluster\n"); break; }
        printf(" -> next is %u\n", next);
        n = next;
    }
    printf("\n  so a file that is not contiguous can still be read. And damage to one FAT\n");
    printf("  removes the way to find the rest of a file --- which is why there are two copies.\n");
    return 0;
}
