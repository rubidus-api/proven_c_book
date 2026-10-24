// Where do the arguments live --- the ones that came in registers, and the
// ones that came on the stack?
//
// C has no syntax for asking "did this argument arrive in a register?".
// But with optimisation off, the compiler spills register-borne arguments
// side by side into its own frame, while stack-borne ones stay where the
// caller put them. The two groups sit far apart, so *measuring the distance
// between neighbouring arguments* reveals the seam.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

static void eight(int a, int b, int c, int d, int e, int f, int g, int h)
{
    const int *arg[8] = { &a, &b, &c, &d, &e, &f, &g, &h };
    ptrdiff_t widest = 0;
    int at = 0;

    for (int i = 0; i + 1 < 8; i++) {
        uintptr_t p = (uintptr_t)arg[i], q = (uintptr_t)arg[i + 1];
        ptrdiff_t gap = p > q ? (ptrdiff_t)(p - q) : (ptrdiff_t)(q - p);
        printf("  argument %d to %d: %td bytes apart\n", i + 1, i + 2, gap);
        if (gap > widest) { widest = gap; at = i + 1; }
    }
    printf("the widest jump is between argument %d and %d\n", at, at + 1);
}

int main(void)
{
    puts("a call with eight arguments:");
    eight(1, 2, 3, 4, 5, 6, 7, 8);
    puts("that jump is the seam between the register group and the stack group.");
    return 0;
}
