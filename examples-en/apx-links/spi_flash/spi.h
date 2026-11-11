/* Driver for the SiFive SPI controller (the one in the FU540 and FE310) and SPI NOR
   flash commands on top. */
#ifndef SPI_H
#define SPI_H
#include <stdint.h>

struct spi { uintptr_t base; };

void spi_init(const struct spi *s, unsigned div);
/* Send n bytes of tx with chip select held down, receiving as many into rx. rx may be
   NULL. */
void spi_xfer(const struct spi *s, const uint8_t *tx, uint8_t *rx, unsigned n);
void spi_release(const struct spi *s);        /* raise chip select: one command ends */

void flash_jedec_id(const struct spi *s, uint8_t id[3]);
void flash_read(const struct spi *s, uint32_t addr, uint8_t *dst, unsigned n);

#endif
