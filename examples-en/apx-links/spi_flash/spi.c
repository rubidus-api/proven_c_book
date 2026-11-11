/* SiFive SPI driver. What Linux's spi-sifive and the FU540's first-stage bootloader do,
   cut down. Hardware is touched only through hw_read32 and hw_write32. */
#include "spi.h"
#include "hw.h"

// [excerpt regs]
enum {
    SCKDIV = 0x00,           /* SPI clock = input clock / (2 x (div + 1)) */
    SCKMODE = 0x04,          /* modes 0-3: the clock's idle level and sampling edge */
    CSID = 0x10, CSMODE = 0x18,
    FMT = 0x40,  /* how many bits at a time, over how many lines, which bit first */
    TXDATA = 0x48,           /* write: into the transmit FIFO. read: bit 31 = full */
    RXDATA = 0x4C,           /* read: from the receive FIFO. bit 31 = empty */
    FCTRL = 0x60,  /* bit 0: the mode that shows the flash as memory addresses */
};
enum { CSMODE_AUTO = 0, CSMODE_HOLD = 2 };
enum { FIFO_FLAG = 1u << 31 };
// [/excerpt]

static uint32_t rd(const struct spi *s, unsigned r)             { return hw_read32(s->base + r); }
static void     wr(const struct spi *s, unsigned r, uint32_t v) { hw_write32(s->base + r, v); }

// [excerpt init]
void spi_init(const struct spi *s, unsigned div)
{
    wr(s, FCTRL, 0);  /* turn off the memory mapping and send commands by hand */
    wr(s, SCKDIV, div);
    wr(s, SCKMODE, 0);            /* mode 0: idle low, sample on the rising edge */
    wr(s, CSID, 0);               /* chip select line 0 = the flash */
    /* 8 bits, one line, most significant bit first, both directions */
    wr(s, FMT, 8u << 16);
}
// [/excerpt]

// [excerpt xfer]
/* SPI receives as much as it sends. Push one byte in and one byte comes out. */
void spi_xfer(const struct spi *s, const uint8_t *tx, uint8_t *rx, unsigned n)
{
    wr(s, CSMODE, CSMODE_HOLD);   /* hold chip select between bytes too */
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
/* A SPI NOR flash command is its first byte.
   One command lasts while chip select is down. */
void flash_jedec_id(const struct spi *s, uint8_t id[3])
{
    uint8_t cmd = 0x9F;  /* READ ID: manufacturer, type, capacity */
    spi_xfer(s, &cmd, 0, 1);
    spi_xfer(s, 0, id, 3);
    spi_release(s);
}

void flash_read(const struct spi *s, uint32_t addr, uint8_t *dst, unsigned n)
{
    uint8_t cmd[4] = { 0x03, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8),
                       (uint8_t)addr };
    spi_xfer(s, cmd, 0, 4);  /* READ: one command byte + 3-byte address */
    spi_xfer(s, 0, dst, n);                   /* then shift out n bytes */
    spi_release(s);
}
// [/excerpt]
