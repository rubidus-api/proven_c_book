/* Two ways to read a 32-bit big-endian value from bytes.
      Neither gives a result that depends on the machine's byte order; (2) handles only
   little- and big-endian machines and stops compiling on any other. */
#include <stdint.h>
#include <string.h>

/* (1) shifts: portable C, nothing outside the standard */
uint32_t get_u32be_shift(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16
         | (uint32_t)p[2] << 8  | (uint32_t)p[3];
}

/* (2) memcpy into a uint32_t, then reverse the bytes if this machine is little endian.
   __builtin_bswap32 is a GCC/Clang extension; MSVC spells it _byteswap_ulong. */
#include <stdbit.h>
uint32_t get_u32be_bswap(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
#if __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__
        v = __builtin_bswap32(v);
#elif __STDC_ENDIAN_NATIVE__ != __STDC_ENDIAN_BIG__
#error "byte order is neither little nor big endian"
#endif
    return v;
}

/* little endian by shifts --- on a little-endian machine this is a plain load */
uint32_t get_u32le_shift(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8
         | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
