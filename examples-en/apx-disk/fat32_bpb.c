/* The first sector *inside* a partition --- build a FAT32 BPB, read it back, and do the
   "cluster number -> sector number" arithmetic for real. Then follow one FAT chain.
   Note: no disk is touched. */
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
    const uint32_t part_lba = 2048;        /* where this partition starts on the disk */
    const uint32_t tot_sec  = 2097152;     /* 1 GiB */
    const uint32_t spc_want = 8;           /* cluster = 8 sectors = 4 KiB */
    const uint32_t rsvd_want = 32, nfat_want = 2;

    /* The size of one FAT is not chosen freely; it *comes out of a calculation*.
       The specification's approximation (FAT32): divide the data sectors by "the sectors one
       cluster takes + the room its FAT entry takes". */
    uint32_t t1 = tot_sec - rsvd_want;
    uint32_t t2 = (256 * spc_want + nfat_want) / 2;
    const uint32_t fat_sz = (t1 + t2 - 1) / t2;

    /* ── fill the BPB as the specification says ─────────────────── */
    bs[0] = 0xeb; bs[1] = 0x58; bs[2] = 0x90;      /* 0  : jump instruction (3 bytes) */
    memcpy(bs + 3, "MSWIN4.1", 8);                 /* 3  : OEM name (8) */
    put16(bs + 11, 512);                           /* 11 : bytes per sector */
    bs[13] = (unsigned char)spc_want;              /* 13 : sectors per cluster */
    put16(bs + 14, (uint16_t)rsvd_want);           /* 14 : reserved sectors (before the FAT) */
    bs[16] = (unsigned char)nfat_want;             /* 16 : number of FATs (usually 2 --- a copy) */
    put16(bs + 17, 0);                             /* 17 : root entries --- 0 on FAT32 */
    put16(bs + 19, 0);                             /* 19 : total sectors (16-bit) --- 0 if unused */
    bs[21] = 0xf8;                                 /* 21 : media type (fixed disk) */
    put16(bs + 22, 0);                             /* 22 : FAT size (16-bit) --- 0 on FAT32 */
    put16(bs + 24, 63);                            /* 24 : sectors per track (old CHS trace) */
    put16(bs + 26, 255);                           /* 26 : heads (old CHS trace) */
    put32(bs + 28, part_lba);                      /* 28 : hidden sectors before this partition */
    put32(bs + 32, tot_sec);                       /* 32 : total sectors (32-bit) */
    put32(bs + 36, fat_sz);                        /* 36 : sectors per FAT (computed) */
    put16(bs + 40, 0);                             /* 40 : FAT mirroring flags */
    put16(bs + 42, 0);                             /* 42 : file system version */
    put32(bs + 44, 2);                             /* 44 : cluster number of the root directory */
    put16(bs + 48, 1);                             /* 48 : FSInfo sector */
    put16(bs + 50, 6);                             /* 50 : where the boot sector copy is */
    bs[64] = 0x80;                                 /* 64 : BIOS drive number */
    bs[66] = 0x29;                                 /* 66 : extended signature --- the three below exist */
    put32(bs + 67, 0x1234ABCDu);                   /* 67 : volume serial number */
    memcpy(bs + 71, "NO NAME    ", 11);            /* 71 : volume label (11) */
    memcpy(bs + 82, "FAT32   ", 8);                /* 82 : file system type label (do not trust it) */
    bs[510] = 0x55; bs[511] = 0xaa;                /* 510: signature */

    /* ── read it back ────────────────────────────────────────── */
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

    /* ── where things are ────────────────────────────────────── */
    uint32_t first_data = rsvd + nfat * fatsz;         /* FAT32 has no separate root directory area */
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
    /* the header line is lined up by hand */
    printf("  cluster   clusters       one FAT      usable as FAT32\n");
    for (uint32_t sc = 1; sc <= 64; sc *= 2) {
        uint32_t a  = tot - rsvd;
        uint32_t b  = (256 * sc + nfat) / 2;
        uint32_t fz = (a + b - 1) / b;                 /* the specification's approximation */
        uint32_t cl = (tot - (rsvd + nfat * fz)) / sc; /* clusters actually left */
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

    /* ── follow a FAT chain ──────────────────────────────────── */
    static unsigned char fat[SEC * 4];      /* imitate only the first four sectors of the FAT */
    put32(fat + 3 * 4, 4);                  /* after 3 comes 4 */
    put32(fat + 4 * 4, 7);                  /* after 4 comes 7 */
    put32(fat + 7 * 4, 0x0FFFFFFFu);        /* 7 is the last */

    printf("\n== when one file is scattered --- the FAT chain ==\n");
    printf("  the FAT is an array of cluster number -> next cluster number (4 bytes each).\n");
    printf("  if a file starts at cluster 3:\n\n");
    uint32_t n = 3;
    for (int step = 0; step < 8; step++) {
        uint32_t next = get32(fat + n * 4) & 0x0FFFFFFFu;   /* the top 4 bits are reserved */
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
