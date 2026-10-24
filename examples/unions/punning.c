/* 같은 비트를 다른 타입으로 보는 세 가지 방법 — 그리고 각각의 계약. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

union bits { float f; uint32_t u; };

static void show(const char *how, uint32_t u)
{
    printf("  %-28s 0x%08X  (sign %u, exponent %3u, significand 0x%06X)\n",
           how, u, u >> 31, (u >> 23) & 0xFFu, u & 0x7FFFFFu);
}

int main(void)
{
    float f = 1.5f;
    printf("reading the bits of the float %.1f as a 32-bit integer\n\n", (double)f);

    /* ① 공용체 — C 에서는 허용된다.
         마지막에 쓴 멤버가 아닌 멤버를 읽으면 '표현을 다시 해석'한다. */
    union bits b = { .f = f };
    show("through a union", b.u);

    /* ② memcpy — 어디서나 계약 안이다. 최적화하면 명령 하나로 접힌다. */
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    show("through memcpy", u);

    /* ③ 포인터 캐스트 — *이것만 계약 밖이다*(엄격한 앨리어싱, 37장).
         값이 같아 보여도 컴파일러가 순서를 바꿀 권리를 갖는다.
         아래 줄은 보이기 위해 적었을 뿐, 쓰면 안 되는 형태다. */
    puts("  through a pointer cast          *(uint32_t *)&f - outside the contract, not shown");

    puts("\n[the other direction is the same]");
    union bits c = { .u = 0x40490FDBu };   /* 원주율에 가까운 비트 */
    printf("  0x40490FDB seen as a float is %.7f\n", (double)c.f);
    float g;
    memcpy(&g, &c.u, sizeof g);
    printf("  moved with memcpy it is also %.7f\n", (double)g);

    puts("\n[where C and C++ part ways]");
    puts("  C   : reading another member of a union is allowed (the representation is reinterpreted).");
    puts("  C++ : reading a member that is not the 'active' one is undefined behaviour.");
    puts("  in a header shared by both languages, memcpy is the safer spelling.");

    puts("\n[careful: not every bit pattern is a value]");
    union bits nan_bits = { .u = 0x7FC00000u };
    printf("  0x7FC00000 → %f (NaN)\n", (double)nan_bits.f);
    puts("  integers and floats are usually harmless, but some types have 'trap representations'.");
    return 0;
}
