/* Driver for the OpenCores I2C master controller (the one in the SiFive FU540 and
   others). */
#ifndef I2C_H
#define I2C_H
#include <stdint.h>

struct i2c {
    uintptr_t base;
    unsigned  shift;  /* register stride: 1 << shift bytes (4 bytes on the FU540) */
    uint32_t  clock;         /* the clock fed to the controller (Hz) */
};

int i2c_init(const struct i2c *b, uint32_t bus_hz);
/* Write w to the device at addr, (repeated start) then read r. -1 on NACK. */
int i2c_write_read(const struct i2c *b, uint8_t addr, const uint8_t *w, unsigned nw,
                   uint8_t *r, unsigned nr);

#endif
