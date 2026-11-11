/* QEMU virt 기계의 16550 을 쓴다. 주소와 클록은 QEMU 가 내주는 장치 트리에 적힌 값이다
   (serial@10000000, clock-frequency = 3686400). */
#include "uart16550.h"
#include "hw.h"

#define FINISH 0x00100000u          /* QEMU 를 끄는 장치(sifive,test0) */

static void say(const struct uart *u, const char *s) { while (*s) uart_putc(u, *s++); }

int main(void)
{
    const struct uart com = { .base = 0x10000000u, .clock = 3686400u };
    if (uart_init(&com, 115200) != 0) return 1;     /* 3686400 / (16 × 115200) = 2 */
    say(&com, uart_loopback_test(&com) == 0 ? "ok\n" : "no\n");
    hw_write32(FINISH, 0x5555u);
    return 0;
}
