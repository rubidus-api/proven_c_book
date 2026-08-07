/* Three ways to view the same bits as another type — and each one's contract. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

union bits { float f; uint32_t u; };

static void show(const char *how, uint32_t u)
{
    printf("  %-28s 0x%08X  (sign %u, exponent %3u, fraction 0x%06X)\n",
           how, u, u >> 31, (u >> 23) & 0xFFu, u & 0x7FFFFFu);
}

int main(void)
{
    float f = 1.5f;
    printf("reading the bits of the float %.1f as a 32-bit integer\n\n", (double)f);

    /* (1) a union — allowed in C.
         Reading a member other than the last one written reinterprets the
         representation. */
    union bits b = { .f = f };
    show("through a union", b.u);

    /* (2) memcpy — inside the contract everywhere; folds to one instruction */
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    show("through memcpy", u);

    /* (3) a pointer cast — *only this one is outside the contract* (strict
         aliasing, chapter 37). The value may look right, but the compiler is
         entitled to reorder. The line below is shown, not used. */
    puts("  through a pointer cast       *(uint32_t *)&f — outside the contract, not run");

    puts("\n[the other direction is the same]");
    union bits c = { .u = 0x40490FDBu };   /* bits close to pi */
    printf("  0x40490FDB seen as a float is %.7f\n", (double)c.f);
    float g;
    memcpy(&g, &c.u, sizeof g);
    printf("  and through memcpy            %.7f\n", (double)g);

    puts("\n[where C and C++ part]");
    puts("  C   : reading another union member is allowed (reinterpretation).");
    puts("  C++ : reading a member that is not the active one is undefined.");
    puts("  In a header shared by both languages, memcpy is the safe spelling.");

    puts("\n[careful: not every bit pattern is a value]");
    union bits nan_bits = { .u = 0x7FC00000u };
    printf("  0x7FC00000 → %f (NaN)\n", (double)nan_bits.f);
    puts("  Integers and floats are usually harmless, but some types have trap");
    puts("  representations that must not be read.");
    return 0;
}
