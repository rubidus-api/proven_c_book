/* SiFive SPI 제어기(FU540 · FE310 의 것)의 드라이버와, 그 위의 SPI NOR 플래시 명령. */
#ifndef SPI_H
#define SPI_H
#include <stdint.h>

struct spi { uintptr_t base; };

void spi_init(const struct spi *s, unsigned div);
/* 칩 선택을 내린 채로 tx 를 n 바이트 보내며 같은 수를 rx 에 받는다. rx 는 NULL 이어도 된다. */
void spi_xfer(const struct spi *s, const uint8_t *tx, uint8_t *rx, unsigned n);
void spi_release(const struct spi *s);        /* 칩 선택을 올린다: 명령 하나가 끝난다 */

void flash_jedec_id(const struct spi *s, uint8_t id[3]);
void flash_read(const struct spi *s, uint32_t addr, uint8_t *dst, unsigned n);

#endif
