/* The print backend: it plays the BIOS on this machine. The disk is the image file run.sh made,
   and each command the boot code gives (read a sector, jump, stop) is printed as one line.
   Characters sent to the serial port are written out as they are. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bios.h"

void mbr_main(uint8_t drive, const struct part *table);
[[noreturn]] void vbr_main(uint8_t drive, const struct part *entry);

static FILE *disk;
static uint8_t low[0x200];           /* 0x0600 --- where the MBR moved itself */
static uint8_t at7c00[0x200];        /* 0x7c00 --- where the BIOS and the MBR load the next stage */

void con_putc(char c) { if (c != '\r') putchar(c); }

int disk_read(uint8_t drive, uint32_t lba, void *dst)
{
    uint16_t to = real_addr(dst);
    if (to == 0xffff) printf("int 13h AH=42h  DL=%02x  LBA %u -> a VBR buffer\n", drive, lba);
    else              printf("int 13h AH=42h  DL=%02x  LBA %u -> %04x\n", drive, lba, to);
    if (fseek(disk, (long)lba * 512, SEEK_SET) != 0) return 1;
    return fread(dst, 512, 1, disk) == 1 ? 0 : 1;
}

uint8_t *load_area(void) { return at7c00; }

uint16_t real_addr(const void *p)
{
    const uint8_t *b = p;
    if (b >= low && b < low + sizeof low) return (uint16_t)(0x0600 + (b - low));
    if (b >= at7c00 && b < at7c00 + sizeof at7c00) return (uint16_t)(0x7c00 + (b - at7c00));
    return 0xffff;                   /* anything else is the VBR's work buffer --- no real-mode address is tracked */
}

/* On the real machine, handing over control is a jump to that address. Here a call to the next
   stage's C function stands in for it --- the loading of the remaining sectors that the VBR's
   first sector does (vboot.c) is skipped. */
void chain(uint8_t drive, const struct part *entry, void *code)
{
    printf("jmp 0000:%04x  DL=%02x  SI=%04x\n", real_addr(code), drive, real_addr(entry));
    vbr_main(drive, entry);
}

void stop(uint8_t code)
{
    printf("out 0xf4 <- %02x  (stop)\n", code);
    fclose(disk);
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(disk = fopen(argv[1], "rb"))) { fprintf(stderr, "usage: trace disk.img\n"); return 1; }
    /* the BIOS's part: load sector 0 at 0x7c00, check the last two bytes, jump with DL=80 */
    if (fread(at7c00, 512, 1, disk) != 1 || at7c00[510] != 0x55 || at7c00[511] != 0xaa) {
        puts("BIOS: LBA 0 is not a boot sector");
        return 0;
    }
    printf("BIOS: LBA 0 -> 7c00, ends in 55 aa; jmp 0000:7c00  DL=80\n");
    memcpy(low, at7c00, sizeof low);  /* start.S's part: move itself to 0x0600 */
    mbr_main(0x80, (const struct part *)(low + 0x1be));
    return 0;
}
