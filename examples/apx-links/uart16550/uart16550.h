/* 16550 UART 드라이버 --- PC 의 COM1 부터 QEMU 의 virt 기계까지 같은 레지스터 여덟 칸. */
#ifndef UART16550_H
#define UART16550_H
#include <stdint.h>

struct uart {
    uintptr_t base;      /* 레지스터 0번의 주소 */
    uint32_t  clock;     /* 칩에 들어오는 클록(Hz). 보율은 이것을 나눠 만든다 */
};

int  uart_init(const struct uart *u, uint32_t baud);   /* 8N1. 못 만드는 보율이면 -1 */
void uart_putc(const struct uart *u, char c);
int  uart_getc(const struct uart *u);                   /* 받은 것이 없으면 -1 */
unsigned uart_errors(const struct uart *u);             /* 줄 상태의 오류 비트 */
int  uart_loopback_test(const struct uart *u);          /* 0 이면 통과 */

#endif
