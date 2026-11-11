/* 진짜 백엔드: 주소를 volatile 포인터로 바꿔 그 자리에 읽고 쓴다.
   volatile 이 빠지면 컴파일러가 「같은 자리를 또 읽을 까닭이 없다」며 폴링 루프를 지운다. */
#include "hw.h"

uint8_t  hw_read8(uintptr_t addr)               { return *(volatile uint8_t *)addr; }
uint32_t hw_read32(uintptr_t addr)              { return *(volatile uint32_t *)addr; }
void     hw_write8(uintptr_t addr, uint8_t v)   { *(volatile uint8_t *)addr = v; }
void     hw_write32(uintptr_t addr, uint32_t v) { *(volatile uint32_t *)addr = v; }

/* 결과 알림은 보드의 콘솔에 바로 쓴다. QEMU virt 는 16550(0x1000_0000),
   sifive_u 는 SiFive UART(0x1001_0000, 보내는 칸의 비트 31 이 「가득 참」)다. */
#ifdef BOARD_SIFIVE_U
void hw_log(const char *s)
{
    volatile uint32_t *tx = (volatile uint32_t *)0x10010000u;
    volatile uint32_t *txctrl = (volatile uint32_t *)0x10010008u;
    *txctrl = 1;                                  /* 송신 켜기 */
    for (; *s; s++) {
        while (*tx & 0x80000000u) { }
        *tx = (uint8_t)*s;
    }
}
#else
void hw_log(const char *s) { while (*s) *(volatile uint8_t *)0x10000000u = (uint8_t)*s++; }
#endif
