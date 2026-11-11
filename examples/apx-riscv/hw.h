/* 하드웨어에 닿는 자리는 이 네 함수뿐이다. 이 부록의 드라이버들은 이것만 부른다.
   QEMU 에서는 hw_mmio.c 가 진짜 주소에 읽고 쓰고,
   보통 PC 에서는 hw_trace.c 가 받은 명령을 찍는다. 드라이버 코드는 한 벌이다.
   hw_log 는 예제가 *제 결과를 알리는* 자리다. 시험하는 장치에 내리는 명령이 아니다. */
#ifndef HW_H
#define HW_H
#include <stdint.h>

uint8_t  hw_read8(uintptr_t addr);
uint32_t hw_read32(uintptr_t addr);
void     hw_write8(uintptr_t addr, uint8_t value);
void     hw_write32(uintptr_t addr, uint32_t value);
void     hw_log(const char *s);

/* printf 가 없는 기계에서 수를 알리는 도우미. */
static inline void hw_log_hex(uint64_t v, int digits)
{
    char b[17];
    for (int i = 0; i < digits; i++)
        b[i] = "0123456789abcdef"[v >> 4 * (digits - 1 - i) & 0xF];
    b[digits] = '\0';
    hw_log(b);
}

static inline void hw_log_dec(uint64_t v)
{
    char b[21];
    int i = 20;
    b[i] = '\0';
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    hw_log(b + i);
}

#endif
