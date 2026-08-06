#include <stdio.h>

int main(void)
{
    int    i = 7;
    double d = 2.5;

    /* The type of the conditional operator is settled as *one* type at compile
       time. If the two sides are int and double, the result is always double. */
    printf("sizeof(int)=%zu sizeof(double)=%zu\n", sizeof i, sizeof d);
    printf("sizeof(1 ? i : d) = %zu  (double even when the condition is true)\n", sizeof(1 ? i : d));
    printf("value  (1 ? i : d) = %.1f\n", 1 ? i : d);   /* 7.0, not 7 */

    /* Mix signedness and the usual arithmetic conversions apply as they are.
       (The line below matches the types up front to avoid a compiler warning —
        leave them unmatched and gcc points at this spot with -Wsign-compare.) */
    int      neg = -1;
    unsigned one = 1u;
    unsigned mixed = 1 ? (unsigned)neg : one;
    printf("(1 ? neg : one) becomes unsigned = %u\n", mixed);

    /* a character constant is already an int in C */
    printf("sizeof('a')=%zu sizeof(1 ? 'a' : 'b')=%zu\n",
           sizeof('a'), sizeof(1 ? 'a' : 'b'));

    /* the pointer rule: if one side is a null pointer constant,
       the result has the type of the other side */
    const char *s = "text";
    const char *r = 1 ? s : NULL;
    printf("pointer branch: %s\n", r);
    return 0;
}
