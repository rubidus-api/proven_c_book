/* SiFive SPI 드라이버. 리눅스의 spi-sifive 와 FU540 의 1단계 부트로더가 하는 일을
   줄여 옮겼다. 하드웨어에 닿는 것은 hw_read32 · hw_write32 뿐이다. */
#include "spi.h"
#include "hw.h"

// [excerpt regs]
enum {
    SCKDIV = 0x00,           /* SPI 클록 = 입력 클록 / (2 × (div + 1)) */
    SCKMODE = 0x04,          /* 모드 0~3: 클록의 쉬는 높이와 찍는 모서리 */
    CSID = 0x10, CSMODE = 0x18,
    FMT = 0x40,              /* 한 번에 몇 비트, 몇 가닥으로, 어느 쪽 비트부터 */
    TXDATA = 0x48,           /* 쓰면 보낼 FIFO 로. 읽으면 비트 31 = 가득 참 */
    RXDATA = 0x4C,           /* 읽으면 받은 FIFO 에서. 비트 31 = 비어 있음 */
    FCTRL = 0x60,            /* 비트 0: 플래시를 메모리 주소로 펼쳐 보이는 방식 */
};
enum { CSMODE_AUTO = 0, CSMODE_HOLD = 2 };
enum { FIFO_FLAG = 1u << 31 };
// [/excerpt]

static uint32_t rd(const struct spi *s, unsigned r)             { return hw_read32(s->base + r); }
static void     wr(const struct spi *s, unsigned r, uint32_t v) { hw_write32(s->base + r, v); }

// [excerpt init]
void spi_init(const struct spi *s, unsigned div)
{
    wr(s, FCTRL, 0);              /* 메모리 펼침을 끄고 명령을 손으로 보낸다 */
    wr(s, SCKDIV, div);
    wr(s, SCKMODE, 0);            /* 모드 0: 쉴 때 낮고, 올라가는 모서리에서 찍는다 */
    wr(s, CSID, 0);               /* 0번 칩 선택 선 = 플래시 */
    wr(s, FMT, 8u << 16);         /* 8비트, 한 가닥, 높은 비트부터, 주고받기 */
}
// [/excerpt]

// [excerpt xfer]
/* SPI 는 보내는 만큼 받는다. 한 바이트를 밀어 넣으면 한 바이트가 밀려 나온다. */
void spi_xfer(const struct spi *s, const uint8_t *tx, uint8_t *rx, unsigned n)
{
    wr(s, CSMODE, CSMODE_HOLD);   /* 바이트 사이에도 칩 선택을 붙들고 있는다 */
    for (unsigned i = 0; i < n; i++) {
        while (rd(s, TXDATA) & FIFO_FLAG) { }
        wr(s, TXDATA, tx ? tx[i] : 0x00);
        uint32_t v;
        while ((v = rd(s, RXDATA)) & FIFO_FLAG) { }
        if (rx) rx[i] = (uint8_t)v;
    }
}

void spi_release(const struct spi *s) { wr(s, CSMODE, CSMODE_AUTO); }
// [/excerpt]

// [excerpt flash]
/* SPI NOR 플래시의 명령은 첫 바이트 하나다.
   칩 선택이 내려가 있는 동안이 명령 하나다. */
void flash_jedec_id(const struct spi *s, uint8_t id[3])
{
    uint8_t cmd = 0x9F;                       /* READ ID: 제조사, 종류, 용량 */
    spi_xfer(s, &cmd, 0, 1);
    spi_xfer(s, 0, id, 3);
    spi_release(s);
}

void flash_read(const struct spi *s, uint32_t addr, uint8_t *dst, unsigned n)
{
    uint8_t cmd[4] = { 0x03, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8),
                       (uint8_t)addr };
    spi_xfer(s, cmd, 0, 4);                   /* READ: 명령 하나 + 3바이트 주소 */
    spi_xfer(s, 0, dst, n);                   /* 이어서 n 바이트를 밀어 내온다 */
    spi_release(s);
}
// [/excerpt]
