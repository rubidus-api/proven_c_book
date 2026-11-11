/* 16550 UART driver. What Linux's 8250 driver and firmware's early consoles do,
   cut down. Hardware is touched only through hw_read8 and hw_write8. */
#include "uart16550.h"
#include "hw.h"

// [excerpt regs]
/* Eight registers of one byte each.
   The same number is a different register for reads and writes. */
enum {
    /* received byte / byte to send / (DLAB=1) divisor low */
    RBR = 0, THR = 0, DLL = 0,
    IER = 1, DLM = 1,            /* interrupt enable / (DLAB=1) divisor high */
    FCR = 2,                     /* FIFO control (write only) */
    LCR = 3,                     /* line control: data bits, parity, stop bits, DLAB */
    MCR = 4,                     /* modem control: DTR, RTS, loopback */
    LSR = 5,                     /* line status: received, room to send, errors */
};
enum { LCR_8N1 = 0x03, LCR_DLAB = 0x80 };
enum { MCR_DTR = 0x01, MCR_RTS = 0x02, MCR_LOOP = 0x10 };
enum { LSR_DR = 0x01, LSR_OE = 0x02, LSR_PE = 0x04, LSR_FE = 0x08, LSR_BI = 0x10,
       LSR_THRE = 0x20 };
// [/excerpt]

static uint8_t rd(const struct uart *u, unsigned r)            { return hw_read8(u->base + r); }
static void    wr(const struct uart *u, unsigned r, uint8_t v) { hw_write8(u->base + r, v); }

// [excerpt init]
int uart_init(const struct uart *u, uint32_t baud)
{
    /* The chip samples each bit 16 times, so divisor = clock / (16 x baud). */
    uint32_t div = u->clock / (16u * baud);
    if (div == 0 || div > 0xFFFF) return -1;
    if (u->clock % (16u * baud) * 50 > 16u * baud) return -1;   /* more than 2% off */

    wr(u, IER, 0x00);                    /* interrupts off; we poll */
    wr(u, LCR, LCR_DLAB);                /* turn slots 0 and 1 into the divisor */
    wr(u, DLL, (uint8_t)div);
    wr(u, DLM, (uint8_t)(div >> 8));
    wr(u, LCR, LCR_8N1);                 /* drop DLAB and set 8N1 */
    wr(u, FCR, 0x07);                    /* enable the FIFOs and clear both */
    wr(u, MCR, MCR_DTR | MCR_RTS);       /* tell the other end "I am here" */
    return 0;
}
// [/excerpt]

// [excerpt io]
void uart_putc(const struct uart *u, char c)
{
    while (!(rd(u, LSR) & LSR_THRE)) { }  /* wait until the transmit slot is empty */
    wr(u, THR, (uint8_t)c);
}

int uart_getc(const struct uart *u)
{
    if (!(rd(u, LSR) & LSR_DR)) return -1;
    return rd(u, RBR);
}

unsigned uart_errors(const struct uart *u)
{
    /* The error bits clear the moment LSR is read. Read once and keep the value. */
    return rd(u, LSR) & (LSR_OE | LSR_PE | LSR_FE | LSR_BI);
}
// [/excerpt]

// [excerpt loop]
/* Connect transmit to receive inside the chip and send one byte round. Before any
   cable, this tells whether the chip is there and its registers obey. */
int uart_loopback_test(const struct uart *u)
{
    uint8_t mcr = rd(u, MCR);
    wr(u, MCR, mcr | MCR_LOOP);
    for (int i = 0; i < 16 && uart_getc(u) >= 0; i++) { }  /* drain the FIFO */
    uart_putc(u, 'K');
    int got = -1;
    for (int tries = 0; tries < 1000 && got < 0; tries++)
        got = uart_getc(u);
    wr(u, MCR, mcr);
    return got == 'K' ? 0 : -1;
}
// [/excerpt]
