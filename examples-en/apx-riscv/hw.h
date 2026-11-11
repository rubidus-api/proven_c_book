/* These four functions are the only places that touch hardware. The drivers of this
   appendix call nothing else. On QEMU, hw_mmio.c reads and writes the real addresses;
   on an ordinary PC, hw_trace.c prints the commands it receives. There is one copy of
   the driver code. hw_log is where an example *reports its own result*. It is not a
   command to the device under test. */
#ifndef HW_H
#define HW_H
#include <stdint.h>

uint8_t  hw_read8(uintptr_t addr);
uint32_t hw_read32(uintptr_t addr);
void     hw_write8(uintptr_t addr, uint8_t value);
void     hw_write32(uintptr_t addr, uint32_t value);
void     hw_log(const char *s);

/* Helpers for reporting numbers on a machine without printf. */
static inline void hw_log_hex(uint64_t v, int digits)
{
    char b[17];
    for (int i = 0; i < digits; i++)
        b[i] = "0123456789abcdef"[v >> 4 * (digits - 1 - i) & 0xF];
    b[digits] = '\0';
    hw_log(b);
}

static inline void hw_log_dec(uint64_t v)
{
    char b[21];
    int i = 20;
    b[i] = '\0';
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    hw_log(b + i);
}

#endif
