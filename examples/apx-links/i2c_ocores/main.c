/* FU540 의 I2C 제어기(0x1003_0000, 레지스터 4바이트 간격)에 달린 온도 감지기 LM75
   (주소 0x48)에서 온도를 읽는다. 주소와 간격은 FU540-C000 설명서의 값이고, 제어기에
   들어오는 클록은 500 MHz 로 둔다. */
#include "i2c.h"
#include "hw.h"

int main(void)
{
    const struct i2c bus = { .base = 0x10030000u, .shift = 2, .clock = 500000000u };
    if (i2c_init(&bus, 100000) != 0) return 1;           /* 100 kHz 표준 속도 */

    uint8_t reg = 0x00, t[2];                            /* 0번 레지스터 = 온도 */
    if (i2c_write_read(&bus, 0x48, &reg, 1, t, 2) != 0) {
        hw_log("no ACK from 0x48\n");
        return 0;
    }
    /* 위 9비트가 0.5 도 단위의 2의 보수다. */
    int half = (int16_t)(t[0] << 8 | t[1]) >> 7;
    hw_log("LM75 at 0x48: ");
    if (half < 0) { hw_log("-"); half = -half; }
    hw_log_dec((uint64_t)half / 2); hw_log(half % 2 ? ".5" : ".0"); hw_log(" C\n");
    return 0;
}
