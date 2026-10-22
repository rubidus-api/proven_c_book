/* The floor near zero — the bits and behaviour of subnormals.
   When the exponent field is all zeros, two rules change: the hidden bit
   becomes 0, and the actual exponent is pinned at 1 - 1023 = -1022, not
   0 - 1023. The bits are read through memcpy (the same way as bits.c). */
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t bits_of(double d)
{
    uint64_t u;
    memcpy(&u, &d, sizeof u);
    return u;
}

static const char *kind(double d)
{
    switch (fpclassify(d)) {
    case FP_ZERO:      return "zero";
    case FP_SUBNORMAL: return "subnormal";
    case FP_NORMAL:    return "normal";
    case FP_INFINITE:  return "infinite";
    default:           return "NaN";
    }
}

/* Significant bits: a normal number always has 53, counting the hidden bit.
   A subnormal keeps only the bits from the leading 1 of the fraction field down. */
static int significant_bits(double d)
{
    uint64_t u = bits_of(d);
    unsigned expo = (unsigned)((u >> 52) & 0x7FFu);
    uint64_t frac = u & 0xFFFFFFFFFFFFFu;
    if (expo != 0) return 53;
    int n = 0;
    while (frac) { n++; frac >>= 1; }
    return n;
}

/* all the bits, exponent field, actual exponent and hidden bit on one line */
static void dump(const char *label, double d)
{
    uint64_t u = bits_of(d);
    unsigned expo = (unsigned)((u >> 52) & 0x7FFu);
    printf("%-15s %016" PRIx64 "  exp %4u (2^%-5d) hidden %d  %-9s %2d bits\n",
           label, u, expo, expo == 0 ? -1022 : (int)expo - 1023,
           expo == 0 ? 0 : 1, kind(d), significant_bits(d));
}

int main(void)
{
    puts("-- the floor of double: both sides of the border --");
    double min_normal = DBL_MIN;                     /* 2^-1022 */
    double max_sub    = nextafter(DBL_MIN, 0.0);     /* the neighbour just below */
    double min_sub    = DBL_TRUE_MIN;                /* 2^-1074 (C11) */
    dump("above DBL_MIN", nextafter(DBL_MIN, 1.0));
    dump("DBL_MIN", min_normal);
    dump("max subnormal", max_sub);
    dump("DBL_TRUE_MIN", min_sub);
    printf("DBL_MIN      = %.17g\n", DBL_MIN);
    printf("DBL_TRUE_MIN = %.17g\n", DBL_TRUE_MIN);
    printf("issubnormal(DBL_MIN) %d, issubnormal(largest subnormal) %d   (C23)\n",
           issubnormal(min_normal) != 0, issubnormal(max_sub) != 0);

    puts("\n-- the step is the same on both sides of the border --");
    printf("DBL_MIN - largest subnormal         == DBL_TRUE_MIN ? %s\n",
           min_normal - max_sub == DBL_TRUE_MIN ? "yes" : "no");
    printf("next above DBL_MIN - DBL_MIN        == DBL_TRUE_MIN ? %s\n",
           nextafter(DBL_MIN, 1.0) - min_normal == DBL_TRUE_MIN ? "yes" : "no");

    puts("\n-- x != y, so x - y is not zero (gradual underflow) --");
    double x = 1.5 * DBL_MIN, y = DBL_MIN;
    double diff = x - y;
    printf("x == y ? %s\n", x == y ? "yes" : "no");
    dump("x - y", diff);
    printf("x - y == 0 ? %s\n", diff == 0.0 ? "yes" : "no");

    puts("\n-- the price: 0.1 pushed down by 2^-k and lifted back --");
    for (int k = 1018; k <= 1074; k += 8) {
        double low = ldexp(0.1, -k);
        printf("k = %4d  %-9s %2d bits  back up: %.17g\n",
               k, kind(low), significant_bits(low), ldexp(low, k));
    }

    puts("\n-- the string that once hung PHP and Java --");
    const char *texts[] = { "2.2250738585072011e-308", "2.2250738585072012e-308" };
    for (int i = 0; i < 2; i++) {
        double d = strtod(texts[i], nullptr);
        printf("%s -> %016" PRIx64 "  %s\n", texts[i], bits_of(d), kind(d));
    }

    puts("\n-- one subnormal row poisons the running sum --");
    /* Of ten rows only row 3 yields a tiny value. Once the sum d has become
       subnormal, every later addition takes a subnormal operand. */
    double d = 0.0;
    int slow = 0;
    for (int row = 0; row < 10; row++) {
        double v = (row == 3) ? 1e-310 : 0.0;
        if (issubnormal(d) || issubnormal(v)) slow++;
        d += v;
    }
    printf("sum %s, additions that touched a subnormal: %d of 10\n", kind(d), slow);
    return 0;
}
