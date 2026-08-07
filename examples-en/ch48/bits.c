/* Opening up a real number — how sign, exponent and fraction really sit in it.
   Type punning goes through memcpy, not a union (the rule of chapters 37 and 46). */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t bits_of(double d)
{
    uint64_t u;
    memcpy(&u, &d, sizeof u);          /* the representation moved as it is */
    return u;
}

static uint32_t bits_of_f(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

/* double: 1 sign + 11 exponent + 52 fraction */
static void dump(const char *label, double d)
{
    uint64_t u = bits_of(d);
    unsigned sign = (unsigned)(u >> 63);
    unsigned expo = (unsigned)((u >> 52) & 0x7FFu);
    uint64_t frac = u & 0xFFFFFFFFFFFFFu;

    printf("%-12s %016" PRIx64 "  sign %u  exp %4u(=%+5d)  frac %013" PRIx64,
           label, u, sign, expo,
           expo == 0 ? -1022 : (int)expo - 1023, frac);

    if (expo == 0x7FF)      printf("  <- %s", frac ? "NaN" : "infinity");
    else if (expo == 0)     printf("  <- %s", frac ? "subnormal" : "zero");
    printf("\n");
}

int main(void)
{
    puts("-- the bits of a double (1 sign + 11 exponent + 52 fraction) --");
    dump("1.0", 1.0);
    dump("-1.0", -1.0);
    dump("0.5", 0.5);
    dump("2.0", 2.0);
    dump("0.1", 0.1);
    dump("0.3", 0.3);
    dump("0.1+0.2", 0.1 + 0.2);
    dump("0.0", 0.0);
    dump("-0.0", -0.0);
    dump("inf", INFINITY);
    dump("NaN", NAN);

    puts("\n-- 0.1 + 0.2 and 0.3 differ in their bits --");
    printf("0.1+0.2 = %.20f\n", 0.1 + 0.2);
    printf("0.3     = %.20f\n", 0.3);
    printf("the bits: %016" PRIx64 " vs %016" PRIx64 "  (the last bit)\n",
           bits_of(0.1 + 0.2), bits_of(0.3));

    puts("\n-- move one ULP from 1.0 and the last digit of the fraction rises by 1 --");
    double one = 1.0;
    double next = nextafter(1.0, 2.0);
    printf("1.0        %016" PRIx64 "\n", bits_of(one));
    printf("next value %016" PRIx64 "  difference %.17g\n", bits_of(next), next - one);
    printf("equal to DBL_EPSILON? %s\n",
           (next - one) == 0x1p-52 ? "yes" : "no");

    puts("\n-- a float uses the same structure, more narrowly (1 + 8 + 23) --");
    float f = 0.1f;
    uint32_t fu = bits_of_f(f);
    printf("0.1f       %08" PRIx32 "  sign %u  exp %3u(=%+4d)  frac %06" PRIx32 "\n",
           fu, fu >> 31, (fu >> 23) & 0xFFu, (int)((fu >> 23) & 0xFFu) - 127,
           fu & 0x7FFFFFu);
    printf("printing (double)0.1f gives %.17g — the trace of narrowing to float\n",
           (double)f);

    puts("\n-- below the smallest normal number come the subnormals --");
    double small = 0x1p-1022;          /* the smallest normal number */
    dump("2^-1022", small);
    dump("half", small / 2);           /* subnormal */
    dump("2^-1074", 0x1p-1074);        /* the smallest subnormal */
    dump("half again", 0x1p-1074 / 2); /* it sinks to zero */
    return 0;
}
