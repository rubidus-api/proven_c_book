/* Use the 16550 of the QEMU virt machine. The address and clock are the values in the
   device tree QEMU provides (serial@10000000, clock-frequency = 3686400). */
#include "uart16550.h"
#include "hw.h"

#define FINISH 0x00100000u          /* the device that shuts QEMU down (sifive,test0) */

static void say(const struct uart *u, const char *s) { while (*s) uart_putc(u, *s++); }

int main(void)
{
    const struct uart com = { .base = 0x10000000u, .clock = 3686400u };
    if (uart_init(&com, 115200) != 0) return 1;     /* 3686400 / (16 × 115200) = 2 */
    say(&com, uart_loopback_test(&com) == 0 ? "ok\n" : "no\n");
    hw_write32(FINISH, 0x5555u);
    return 0;
}
