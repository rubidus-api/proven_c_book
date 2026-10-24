/* 같은 비트를 다른 타입으로 읽는 두 가지 *계약 안의* 길 --- memcpy 와 공용체.
 * 포인터 캐스트(*(uint32_t *)&f)는 엄격한 앨리어싱을 어긴다. 이 둘은 어기지 않는다. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t bits_by_memcpy(float f)
{
    uint32_t u;
    static_assert(sizeof u == sizeof f, "float must be 32 bits here");
    memcpy(&u, &f, sizeof u);       /* 바이트를 옮길 뿐이다 --- 최적화되면 명령 하나 */
    return u;
}

static uint32_t bits_by_union(float f)
{
    union { float f; uint32_t u; } pun = { .f = f };
    return pun.u;                   /* 다른 멤버로 읽기: C 에서는 허용(C23 6.5.3.4 각주) */
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
