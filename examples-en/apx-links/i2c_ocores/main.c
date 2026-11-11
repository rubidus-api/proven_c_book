/* Read the temperature from an LM75 sensor (address 0x48) on the FU540's I2C controller
   (0x1003_0000, registers 4 bytes apart). Address and stride are from the FU540-C000
   manual; the controller's input clock is taken as 500 MHz. */
#include "i2c.h"
#include "hw.h"

int main(void)
{
    const struct i2c bus = { .base = 0x10030000u, .shift = 2, .clock = 500000000u };
    if (i2c_init(&bus, 100000) != 0) return 1;           /* 100 kHz standard mode */

    uint8_t reg = 0x00, t[2];                            /* register 0 = temperature */
    if (i2c_write_read(&bus, 0x48, &reg, 1, t, 2) != 0) {
        hw_log("no ACK from 0x48\n");
        return 0;
    }
    /* The top 9 bits are two's complement in units of 0.5 degrees. */
    int half = (int16_t)(t[0] << 8 | t[1]) >> 7;
    hw_log("LM75 at 0x48: ");
    if (half < 0) { hw_log("-"); half = -half; }
    hw_log_dec((uint64_t)half / 2); hw_log(half % 2 ? ".5" : ".0"); hw_log(" C\n");
    return 0;
}
