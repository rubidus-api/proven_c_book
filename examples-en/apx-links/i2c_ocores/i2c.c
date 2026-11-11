/* OpenCores I2C driver. What Linux's i2c-ocores does, cut down to polling. The
   controller's command bits match the I2C signals (start, byte, ACK, stop) one for one.
   Hardware is touched only through hw_read8 and hw_write8. */
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
    /* The controller runs at five times the bus clock: prescale = clock / (5 x bus) -
       1. */
    uint32_t pre = b->clock / (5 * bus_hz) - 1;
    if (pre > 0xFFFF) return -1;
    wr(b, CTR, 0);  /* the prescaler can only be changed while disabled */
    wr(b, PRERLO, (uint8_t)pre);
    wr(b, PRERHI, (uint8_t)(pre >> 8));
    wr(b, CTR, CTR_EN);
    return 0;
}
// [/excerpt]

// [excerpt step]
/* Issue one command and wait for it to finish.
   One byte plus one ACK bit go over the wire. */
static int step(const struct i2c *b, uint8_t cmd)
{
    wr(b, CR, cmd);
    uint8_t sr;
    while ((sr = rd(b, SR)) & SR_TIP) { }
    if (sr & SR_AL) return -1;                     /* lost the bus to another master */
    return (cmd & CR_WR) && (sr & SR_RXNACK) ? -1 : 0;   /* nobody ACKed */
}
// [/excerpt]

// [excerpt xfer]
int i2c_write_read(const struct i2c *b, uint8_t addr, const uint8_t *w, unsigned nw,
                   uint8_t *r, unsigned nr)
{
    wr(b, TXR, (uint8_t)(addr << 1));                    /* address + write (0) */
    if (step(b, CR_STA | CR_WR)) goto fail;
    for (unsigned i = 0; i < nw; i++) {
        wr(b, TXR, w[i]);
        if (step(b, CR_WR)) goto fail;
    }
    if (nr) {
        wr(b, TXR, (uint8_t)(addr << 1 | 1));  /* repeated start, address + read (1) */
        if (step(b, CR_STA | CR_WR)) goto fail;
        for (unsigned i = 0; i < nr; i++) {
            /* the last byte is answered with NACK ("enough"), and a stop follows */
            int last = i + 1 == nr;
            step(b, (uint8_t)(CR_RD | (last ? CR_NACK | CR_STO : 0)));
            r[i] = rd(b, RXR);
        }
        return 0;
    }
    return step(b, CR_STO);
fail:
    wr(b, CR, CR_STO);                                   /* release the bus */
    return -1;
}
// [/excerpt]
