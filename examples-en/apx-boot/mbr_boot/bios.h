/* The boot code touches the machine only through the functions in this header.
   On QEMU's PC machine bios_int.c really calls the BIOS's int 13h and the serial port;
   on an ordinary PC bios_trace.c prints the commands it is given. There is one copy of the MBR
   and VBR code. */
#ifndef BIOS_H
#define BIOS_H
#include <stdint.h>

/* One entry of the partition table, 16 bytes. The table starts at 0x1be, not on a 4-byte
   boundary, so the alignment requirement is lowered to 1 (GCC's packed) and the struct can be
   laid over any byte. x86 hardware accepts a misaligned read, but C does not promise it. */
struct [[gnu::packed]] part {
    uint8_t  boot;           /* 0x80 means "boot from here" */
    uint8_t  chs_first[3];   /* old CHS address --- today's tools fill it in for form's sake */
    uint8_t  type;           /* 0x83 Linux, 0x82 swap, 0xee "a disk covered by GPT" */
    uint8_t  chs_last[3];
    uint32_t lba;            /* first sector --- this is what is really used */
    uint32_t count;          /* number of sectors */
};
static_assert(sizeof(struct part) == 16);

void     con_putc(char c);                                   /* one character to the serial port */
int      disk_read(uint8_t drive, uint32_t lba, void *dst);  /* one sector, 0 on success */
uint8_t *load_area(void);                                    /* where the next stage goes, 0x7c00 */
uint16_t real_addr(const void *p);                           /* the address as real mode sees it */
[[noreturn]] void chain(uint8_t drive, const struct part *entry, void *code);
[[noreturn]] void stop(uint8_t code);                        /* 0 means it got to the end */

#endif
