/* OpenCores I2C 드라이버. 리눅스의 i2c-ocores 가 하는 일을 폴링으로 줄여 옮겼다.
   제어기의 명령 비트가 I2C 의 신호(시작 · 바이트 · ACK · 정지)와 하나씩 맞는다.
   하드웨어에 닿는 것은 hw_read8 · hw_write8 뿐이다. */
#include "i2c.h"
#include "hw.h"

// [excerpt regs]
enum { PRERLO = 0, PRERHI = 1, CTR = 2, TXR = 3, RXR = 3, CR = 4, SR = 4 };
enum { CTR_EN = 0x80 };
enum { CR_STA = 0x80, CR_STO = 0x40, CR_RD = 0x20, CR_WR = 0x10, CR_NACK = 0x08 };
enum { SR_RXNACK = 0x80, SR_AL = 0x20, SR_TIP = 0x02 };
// [/excerpt]

static uint8_t rd(const struct i2c *b, unsigned r)
{ return hw_read8(b->base + (r << b->shift)); }
static void wr(const struct i2c *b, unsigned r, uint8_t v)
{ hw_write8(b->base + (r << b->shift), v); }

// [excerpt init]
int i2c_init(const struct i2c *b, uint32_t bus_hz)
{
    /* 제어기는 버스 클록의 다섯 배로 돈다: 분주값 = 클록 / (5 × 버스) - 1. */
    uint32_t pre = b->clock / (5 * bus_hz) - 1;
    if (pre > 0xFFFF) return -1;
    wr(b, CTR, 0);                       /* 분주값은 꺼 둔 채로만 바꿀 수 있다 */
    wr(b, PRERLO, (uint8_t)pre);
    wr(b, PRERHI, (uint8_t)(pre >> 8));
    wr(b, CTR, CTR_EN);
    return 0;
}
// [/excerpt]

// [excerpt step]
/* 명령 하나를 내리고 끝나기를 기다린다.
   바이트 하나 + ACK 한 비트가 선 위에서 오간다. */
static int step(const struct i2c *b, uint8_t cmd)
{
    wr(b, CR, cmd);
    uint8_t sr;
    while ((sr = rd(b, SR)) & SR_TIP) { }
    if (sr & SR_AL) return -1;                     /* 다른 주인에게 버스를 잃었다 */
    return (cmd & CR_WR) && (sr & SR_RXNACK) ? -1 : 0;   /* 아무도 ACK 하지 않았다 */
}
// [/excerpt]

// [excerpt xfer]
int i2c_write_read(const struct i2c *b, uint8_t addr, const uint8_t *w, unsigned nw,
                   uint8_t *r, unsigned nr)
{
    wr(b, TXR, (uint8_t)(addr << 1));                    /* 주소 + 쓰기(0) */
    if (step(b, CR_STA | CR_WR)) goto fail;
    for (unsigned i = 0; i < nw; i++) {
        wr(b, TXR, w[i]);
        if (step(b, CR_WR)) goto fail;
    }
    if (nr) {
        wr(b, TXR, (uint8_t)(addr << 1 | 1));            /* 다시 시작, 주소 + 읽기(1) */
        if (step(b, CR_STA | CR_WR)) goto fail;
        for (unsigned i = 0; i < nr; i++) {
            /* 마지막 바이트에는 NACK 으로 「그만」이라 답하고 정지를 붙인다 */
            int last = i + 1 == nr;
            step(b, (uint8_t)(CR_RD | (last ? CR_NACK | CR_STO : 0)));
            r[i] = rd(b, RXR);
        }
        return 0;
    }
    return step(b, CR_STO);
fail:
    wr(b, CR, CR_STO);                                   /* 버스를 놓아 준다 */
    return -1;
}
// [/excerpt]
