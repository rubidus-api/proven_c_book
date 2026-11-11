/* 16550 UART driver --- the same eight registers from the PC's COM1 to QEMU's virt
   machine. */
#ifndef UART16550_H
#define UART16550_H
#include <stdint.h>

struct uart {
    uintptr_t base;      /* address of register 0 */
    /* the clock fed to the chip (Hz). Baud rates are made by dividing it */
    uint32_t  clock;
};

/* 8N1. -1 if the baud rate cannot be made */
int  uart_init(const struct uart *u, uint32_t baud);
void uart_putc(const struct uart *u, char c);
int  uart_getc(const struct uart *u);  /* -1 if nothing has been received */
unsigned uart_errors(const struct uart *u);  /* the error bits of the line status */
int  uart_loopback_test(const struct uart *u);          /* 0 on pass */

#endif
