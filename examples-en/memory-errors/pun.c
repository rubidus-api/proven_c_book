/* Two roads *inside the contract* to read the same bits as another type --- memcpy and a union.
 * A pointer cast (*(uint32_t *)&f) breaks strict aliasing. These two do not. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t bits_by_memcpy(float f)
{
    uint32_t u;
    static_assert(sizeof u == sizeof f, "float must be 32 bits here");
    memcpy(&u, &f, sizeof u);       /* only moves bytes --- one instruction once optimised */
    return u;
}

static uint32_t bits_by_union(float f)
{
    union { float f; uint32_t u; } pun = { .f = f };
    return pun.u;                   /* reading another member: allowed in C (C23 6.5.3.4 footnote) */
}

int main(void)
{
    const float samples[] = {1.0f, -2.5f, 0.1f};
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; i++) {
        float f = samples[i];
        printf("%5.2f  memcpy 0x%08x  union 0x%08x\n",
               (double)f, (unsigned)bits_by_memcpy(f), (unsigned)bits_by_union(f));
    }
    return 0;
}
