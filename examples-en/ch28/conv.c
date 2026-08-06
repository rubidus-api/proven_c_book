#include <stdint.h>
#include <stdio.h>

int main(void)
{
    /* integer promotion: char and short are widened to int before the arithmetic */
    signed char a = 100;
    signed char b = 100;
    printf("100 + 100 (as chars) = %d\n", a + b);   /* 200 — a value a char cannot hold */

    /* the usual arithmetic conversions: the unsigned side wins —
       read -1 through unsigned eyes and it becomes a huge positive number */
    int neg = -1;
    printf("(unsigned)(-1)   = %u\n", (unsigned)neg);
    printf("so -1 < 1u is false\n");

    /* integer division vs real division — the cast states the intent */
    int total = 7, count = 2;
    printf("7 / 2        = %d\n", total / count);
    printf("(double)7/2  = %.1f\n", (double)total / count);

    /* the default promotions of variadic arguments: float to double, char/short to int */
    float f = 1.5f;
    printf("float 1.5f via %%f = %f  (promoted to double)\n", f);
    return 0;
}
