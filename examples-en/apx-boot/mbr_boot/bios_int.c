/* The real backend: it calls the BIOS's int 13h and the serial port. Built as 16-bit real-mode
   code (gcc -m16). Run on QEMU's PC machine with SeaBIOS; not run on a real PC's firmware. */
#include "bios.h"

static inline void outb(uint16_t port, uint8_t v)
{ __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port)); }
static inline uint8_t inb(uint16_t port)
{ uint8_t v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }

// [excerpt int13]
/* COM1: one byte once the transmit slot is empty (LSR bit 5) */
void con_putc(char c)
{
    while (!(inb(0x3fd) & 0x20)) { }
    outb(0x3f8, (uint8_t)c);
}

int disk_read(uint8_t drive, uint32_t lba, void *dst)
{
    /* the 16 bytes for int 13h's extended read (AH=42h) */
    struct [[gnu::packed]] {
        uint8_t  size, zero;
        uint16_t count, offset, segment;
        uint32_t lba_low, lba_high;
    } dap = { 16, 0, 1, (uint16_t)(uintptr_t)dst, 0, lba, 0 };
    uint16_t ax = 0x4200;
    uint8_t failed;
    /* on failure the BIOS sets CF */
    __asm__ volatile("int $0x13; setc %1"
                     : "+a"(ax), "=qm"(failed)
                     : "d"(drive), "S"(&dap)
                     : "memory", "cc");
    return failed;
}

void chain(uint8_t drive, const struct part *entry, void *code)
{
    __asm__ volatile("jmp *%2" : : "d"(drive), "S"(entry), "r"(code));
    __builtin_unreachable();
}
// [/excerpt]

uint8_t *load_area(void) { return (uint8_t *)0x7c00; }
uint16_t real_addr(const void *p) { return (uint16_t)(uintptr_t)p; }

/* A write to QEMU's isa-debug-exit device (port 0xf4) ends the emulator --- it only ends the
   demonstration; a real PC would call int 18h ("try the next device") here or halt. */
void stop(uint8_t code)
{
    outb(0xf4, code);
    for (;;) __asm__ volatile("hlt");
}
