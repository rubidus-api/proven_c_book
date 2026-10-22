/* What is in a disk's first sector --- build one, then read it back.

   Note: this program does not touch any disk. It opens no file or device and writes
   nothing. It fills one array in memory by the old conventions, reads it back, decodes
   it and prints. Run it and you get text, nothing more.

   The bytes built here follow the real rules: 512 bytes, four partition slots at 0x1BE,
   and 0x55 0xAA at the end. It also builds a GPT disk's first sector (the protective MBR)
   and the EFI PART header. It never writes these bytes to a real sector 0. */
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
    e[0] = boot;              /* 0x80 means "boot from this one" */
    e[1] = 0xfe; e[2] = 0xff; e[3] = 0xff;   /* the old CHS fields --- only filled in for form now */
    e[4] = type;
    e[5] = 0xfe; e[6] = 0xff; e[7] = 0xff;
    put32(e + 8, lba);        /* start --- this is what is really used */
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

    /* (1) The bootstrap area --- 446 bytes. A real boot loader's first piece goes here. */
    memcpy(mbr, "\xfa\x31\xc0\x8e\xd8\x8e\xd0\xbc\x00\x7c", 10);  /* cli; set up segments and stack */
    printf("how the first sector divides up\n");
    printf("  0x000 - 0x1bd : bootstrap code, %d bytes\n", 0x1be);
    printf("  0x1be - 0x1fd : partition table --- 16 bytes x 4 entries\n");
    printf("  0x1fe - 0x1ff : signature 0x55 0xaa\n\n");

    /* (2) The partition table --- one common layout */
    put_part(mbr + 0x1be, 0x80, 0x83, 2048, 1024000);   /* bootable, Linux */
    put_part(mbr + 0x1ce, 0x00, 0x82, 1026048, 262144); /* Linux swap */
    mbr[510] = 0x55; mbr[511] = 0xaa;

    printf("the partition table (from 0x1be)\n");
    hexdump(mbr, 0x1be, 32);
    printf("\n  entry 1: bootable %s · type 0x%02x (Linux) · start LBA %u · %u sectors (%u MiB)\n",
           mbr[0x1be] == 0x80 ? "yes" : "no", mbr[0x1be + 4],
           2048u, 1024000u, 1024000u / 2048u);
    printf("  entry 2: bootable %s · type 0x%02x (swap) · start LBA %u\n\n",
           mbr[0x1ce] == 0x80 ? "yes" : "no", mbr[0x1ce + 4], 1026048u);

    printf("the last two bytes: %02x %02x --- %s\n\n", mbr[510], mbr[511],
           (mbr[510] == 0x55 && mbr[511] == 0xaa)
           ? "the BIOS accepts this as a boot sector only with these" : "not a boot sector");

    /* (3) The first sector of a GPT disk --- the protective MBR */
    unsigned char pmbr[512] = { 0 };
    put_part(pmbr + 0x1be, 0x00, 0xee, 1, 0xffffffffu);  /* 0xEE = "from here to the end belongs to someone else" */
    pmbr[510] = 0x55; pmbr[511] = 0xaa;
    printf("the protective MBR (first sector of a GPT disk)\n");
    printf("  partition type 0x%02x --- it tells old tools the disk is fully used\n",
           pmbr[0x1be + 4]);
    printf("  the reason: to stop a tool that does not know GPT taking it for empty\n\n");

    /* (4) The real map is in the second sector (LBA 1) --- the GPT header */
    unsigned char gpt[92] = { 0 };
    memcpy(gpt, "EFI PART", 8);
    put32(gpt + 8, 0x00010000u);      /* revision 1.0 */
    put32(gpt + 12, 92);              /* header size */
    put32(gpt + 16, 0);               /* the CRC field is left 0 while computing */
    put32(gpt + 16, crc32(gpt, 92));

    printf("the GPT header (LBA 1)\n");
    printf("  signature : %.8s\n", gpt);
    printf("  header CRC32: 0x%08x --- it checks itself (this field is zero while computing)\n",
           (unsigned)(gpt[16] | gpt[17] << 8 | gpt[18] << 16 | (uint32_t)gpt[19] << 24));
    printf("  and the same table exists once more at the very end of the disk, to survive damage\n");
    return 0;
}
