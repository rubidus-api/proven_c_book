/* A look inside the container that holds fractions --- why 0.1 + 0.2 is not 0.3.
   The values printed here are the same on any machine that follows IEEE 754. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Print the bits of a double, split into sign, exponent and fraction */
static void show(const char *label, double v)
{
    uint64_t u;
    memcpy(&u, &v, sizeof u);
    printf("%-10s %.17g\n", label, v);
    printf("           sign %u  exponent %4u  fraction 0x%013llx\n",
           (unsigned)(u >> 63), (unsigned)((u >> 52) & 0x7FF),
           (unsigned long long)(u & 0xFFFFFFFFFFFFFULL));
}

int main(void)
{
    puts("== the famous one ==");
    printf("0.1 + 0.2 == 0.3 ? %s\n", (0.1 + 0.2 == 0.3) ? "yes" : "no");
    printf("0.1 + 0.2 = %.17g\n", 0.1 + 0.2);
    printf("0.3       = %.17g\n\n", 0.3);

    puts("== why: 0.1 has no exact form in base two ==");
    show("0.1", 0.1);
    show("0.2", 0.2);
    show("0.5", 0.5);
    puts("0.5 is exact (fraction is all zeros); 0.1 and 0.2 are not.\n");

    puts("== the error piles up ==");
    double sum = 0.0;
    for (int i = 0; i < 10; i++) sum += 0.1;
    printf("adding 0.1 ten times = %.17g\n", sum);
    printf("1.0                  = %.17g\n", 1.0);
    printf("difference           = %.3g\n\n", sum - 1.0);

    puts("== a narrower container loses more ==");
    float  f = 0.1f;
    double d = 0.1;
    printf("0.1 as float  = %.17g\n", (double)f);
    printf("0.1 as double = %.17g\n\n", d);

    puts("== and some numbers are not numbers ==");
    double inf = 1.0 / 0.0e0, nan = 0.0 / 0.0e0;
    printf("1.0 / 0.0 = %g\n", inf);
    printf("0.0 / 0.0 = %g\n", nan);
    printf("nan == nan ? %s   (the only value not equal to itself)\n",
           (nan == nan) ? "yes" : "no");
    return 0;
}
