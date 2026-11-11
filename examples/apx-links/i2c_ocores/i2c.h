/* OpenCores I2C 주인 제어기(SiFive FU540 등에 들어 있는 것)의 드라이버. */
#ifndef I2C_H
#define I2C_H
#include <stdint.h>

struct i2c {
    uintptr_t base;
    unsigned  shift;         /* 레지스터 간격: 1 << shift 바이트(FU540 은 4바이트 간격) */
    uint32_t  clock;         /* 제어기에 들어오는 클록(Hz) */
};

int i2c_init(const struct i2c *b, uint32_t bus_hz);
/* 주소 addr 의 장치에 w 를 쓰고(다시 시작), r 을 읽는다. NACK 이면 -1. */
int i2c_write_read(const struct i2c *b, uint8_t addr, const uint8_t *w, unsigned nw,
                   uint8_t *r, unsigned nr);

#endif
