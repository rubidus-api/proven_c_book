/* 16550 UART 드라이버. 리눅스의 8250 드라이버와 펌웨어의 초기 콘솔이 하는 일을
   줄여 옮겼다. 하드웨어에 닿는 것은 hw_read8 · hw_write8 뿐이다. */
#include "uart16550.h"
#include "hw.h"

// [excerpt regs]
/* 레지스터는 바이트 하나씩 여덟 칸이다.
   같은 번호가 읽을 때와 쓸 때 다른 레지스터다. */
enum {
    RBR = 0, THR = 0, DLL = 0,   /* 받은 바이트 / 보낼 바이트 / (DLAB=1) 분주값 아래 */
    IER = 1, DLM = 1,            /* 인터럽트 허용 / (DLAB=1) 분주값 위 */
    FCR = 2,                     /* FIFO 제어(쓰기 전용) */
    LCR = 3,                     /* 줄 제어: 자료 비트 · 패리티 · 정지 비트 · DLAB */
    MCR = 4,                     /* 모뎀 제어: DTR · RTS · 루프백 */
    LSR = 5,                     /* 줄 상태: 받은 것 있음 · 보낼 자리 있음 · 오류 */
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
    /* 칩은 한 비트를 16번 찍어 읽는다. 그래서 분주값 = 클록 / (16 × 보율). */
    uint32_t div = u->clock / (16u * baud);
    if (div == 0 || div > 0xFFFF) return -1;
    if (u->clock % (16u * baud) * 50 > 16u * baud) return -1;   /* 오차 2% 넘음 */

    wr(u, IER, 0x00);                    /* 인터럽트는 끄고 폴링으로 간다 */
    wr(u, LCR, LCR_DLAB);                /* 0번 · 1번 칸을 분주값 자리로 바꾼다 */
    wr(u, DLL, (uint8_t)div);
    wr(u, DLM, (uint8_t)(div >> 8));
    wr(u, LCR, LCR_8N1);                 /* DLAB 을 내리며 8N1 로 */
    wr(u, FCR, 0x07);                    /* FIFO 를 켜고 양쪽을 비운다 */
    wr(u, MCR, MCR_DTR | MCR_RTS);       /* 「여기 있다」를 상대에게 알린다 */
    return 0;
}
// [/excerpt]

// [excerpt io]
void uart_putc(const struct uart *u, char c)
{
    while (!(rd(u, LSR) & LSR_THRE)) { }  /* 보낼 칸이 빌 때까지 기다린다 */
    wr(u, THR, (uint8_t)c);
}

int uart_getc(const struct uart *u)
{
    if (!(rd(u, LSR) & LSR_DR)) return -1;
    return rd(u, RBR);
}

unsigned uart_errors(const struct uart *u)
{
    /* 오류 비트는 LSR 을 읽는 순간 지워진다. 한 번 읽어 간직해야 한다. */
    return rd(u, LSR) & (LSR_OE | LSR_PE | LSR_FE | LSR_BI);
}
// [/excerpt]

// [excerpt loop]
/* 칩 안에서 송신을 수신에 잇고 한 바이트를 돌려 본다. 선을 꽂기 전에
   「칩이 거기 있고 레지스터가 말을 듣는가」를 가린다. */
int uart_loopback_test(const struct uart *u)
{
    uint8_t mcr = rd(u, MCR);
    wr(u, MCR, mcr | MCR_LOOP);
    for (int i = 0; i < 16 && uart_getc(u) >= 0; i++) { }  /* FIFO 를 비운다 */
    uart_putc(u, 'K');
    int got = -1;
    for (int tries = 0; tries < 1000 && got < 0; tries++)
        got = uart_getc(u);
    wr(u, MCR, mcr);
    return got == 'K' ? 0 : -1;
}
// [/excerpt]
