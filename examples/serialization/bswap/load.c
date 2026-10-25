/* 바이트에서 32비트 빅 엔디안 값을 읽는 두 방법.
   어느 쪽도 기계 자신의 바이트 순서에 기대지 않는다. */
#include <stdint.h>
#include <string.h>

/* (1) 시프트: 이식되는 C, 표준 밖의 것이 없다 */
uint32_t get_u32be_shift(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16
         | (uint32_t)p[2] << 8  | (uint32_t)p[3];
}

/* (2) uint32_t 에 memcpy 한 뒤, 이 기계가 리틀 엔디안이면 바이트를 뒤집는다.
   __builtin_bswap32 는 GCC·Clang 확장이다. MSVC 에서는 _byteswap_ulong. */
#include <stdbit.h>
uint32_t get_u32be_bswap(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
#if __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__
    v = __builtin_bswap32(v);
#endif
    return v;
}

/* 시프트로 리틀 엔디안 --- 리틀 엔디안 기계에서는 그냥 읽기 하나 */
uint32_t get_u32le_shift(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8
         | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
