/* The real backend: turn the address into a volatile pointer and read or write there.
   Without volatile the compiler would delete polling loops ("why read the same place
   again?"). */
#include "hw.h"

uint8_t  hw_read8(uintptr_t addr)               { return *(volatile uint8_t *)addr; }
uint32_t hw_read32(uintptr_t addr)              { return *(volatile uint32_t *)addr; }
void     hw_write8(uintptr_t addr, uint8_t v)   { *(volatile uint8_t *)addr = v; }
void     hw_write32(uintptr_t addr, uint32_t v) { *(volatile uint32_t *)addr = v; }

/* Reports go straight to the board's console: on QEMU virt the 16550 (0x1000_0000), on
   sifive_u the SiFive UART (0x1001_0000, bit 31 of the transmit slot means "full"). */
#ifdef BOARD_SIFIVE_U
void hw_log(const char *s)
{
    volatile uint32_t *tx = (volatile uint32_t *)0x10010000u;
    volatile uint32_t *txctrl = (volatile uint32_t *)0x10010008u;
    *txctrl = 1;                                  /* enable transmit */
    for (; *s; s++) {
        while (*tx & 0x80000000u) { }
        *tx = (uint8_t)*s;
    }
}
#else
void hw_log(const char *s) { while (*s) *(volatile uint8_t *)0x10000000u = (uint8_t)*s++; }
#endif
