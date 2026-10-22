/* RISC-V bare metal: arm a timer interrupt; the handler only raises a flag and leaves.
   Really run on QEMU's virt machine and the output captured
   (UART 0x1000_0000, CLINT 0x0200_0000, exit device 0x0010_0000). */
#include <stdint.h>

#define UART0   ((volatile uint8_t  *)0x10000000u)
#define MTIME   ((volatile uint64_t *)0x0200bff8u)
#define MTIMECMP ((volatile uint64_t *)0x02004000u)
#define FINISH  ((volatile uint32_t *)0x00100000u)  /* where QEMU is switched off */
#define TICK    10000000u                    /* 1 second at 10 MHz */

static volatile uint32_t ticks = 0;          /* a value the handler and the main loop both see */

static void uart_puts(const char *s) { while (*s) *UART0 = (uint8_t)*s++; }

/* The hardware saves no registers at all, so we make the compiler do it.
   What this attribute does: save and restore every register used, and end with mret, not ret.

   ★ Forget aligned(4) and it breaks silently. mtvec's low two bits are not address but
   the MODE field. With compressed instructions (the C extension) a function can sit on a
   2-byte boundary; write that address to mtvec and MODE becomes 2 (reserved) --- traps go astray. */
__attribute__((interrupt("machine"), aligned(4)))
void trap_handler(void)
{
    *MTIMECMP = *MTIME + TICK;               /* schedule the next alarm */
    ticks++;                                 /* only raise the flag and leave at once */
}

int main(void)
{
    extern void trap_handler(void);
    uintptr_t vec = (uintptr_t)&trap_handler;
    __asm__ volatile ("csrw mtvec, %0" :: "r"(vec));       /* traps come here */
    *MTIMECMP = *MTIME + TICK;
    __asm__ volatile ("csrs mie, %0"    :: "r"(1u << 7));  /* enable the timer interrupt */
    __asm__ volatile ("csrs mstatus, %0" :: "r"(1u << 3)); /* global enable (MIE) */

    uart_puts("waiting for the timer...\n");
    uint32_t seen = 0;
    for (;;) {
        if (ticks != seen) {                 /* the main loop does the work */
            seen = ticks;
            uart_puts("tick\n");
            if (seen == 3) {
                uart_puts("three ticks; done\n");
                *FINISH = 0x5555u;           /* switch the emulator off */
            }
        }
        __asm__ volatile ("wfi");            /* sleep when there is nothing to do */
    }
    for (;;) { }                             /* nowhere to return to */
}
