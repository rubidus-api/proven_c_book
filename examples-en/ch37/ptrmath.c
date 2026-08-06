/* Pointer arithmetic moves in "slots", not in bytes. */
#include <stddef.h>
#include <stdio.h>

struct point { int x, y; };

int main(void)
{
    char          c[4];
    int           a[8] = {0, 10, 20, 30, 40, 50, 60, 70};
    double        d[4];
    struct point  s[4];

    /* ── (1) the same +1, a different distance moved ─────────────── */
    printf("type           sizeof   (p+1) - p in bytes\n");
    printf("char           %2zu       %td\n",
           sizeof c[0], (char *)(c + 1) - (char *)c);
    printf("int            %2zu       %td\n",
           sizeof a[0], (char *)(a + 1) - (char *)a);
    printf("double         %2zu       %td\n",
           sizeof d[0], (char *)(d + 1) - (char *)d);
    printf("struct point   %2zu       %td\n",
           sizeof s[0], (char *)(s + 1) - (char *)s);

    /* to move by bytes, cast to a character pointer */
    int *p = a;
    printf("\np + 1        points at %d\n", *(p + 1));
    printf("(char*)p + 1 is the second byte of a[0]\n");

    /* ── (2) pointer subtraction gives a count of elements ────────── */
    ptrdiff_t gap = &a[4] - &a[1];
    printf("\n&a[4] - &a[1]        = %td  (elements, not bytes)\n", gap);
    printf("counted in bytes     = %td\n",
           (char *)&a[4] - (char *)&a[1]);

    /* ── (3) the subscript is sugar over the arithmetic ───────────── */
    printf("\na[3] = %d, *(a + 3) = %d, *(3 + a) = %d, 3[a] = %d\n",
           a[3], *(a + 3), *(3 + a), 3[a]);

    /* ── (4) one past the end may be formed, but never followed ───── */
    int *end = a + 8;                 /* one-past-the-end — legal */
    printf("\nelements up to the one-past-the-end address: %td (never dereferenced)\n", end - a);

    int count = 0;
    for (int *q = a; q != end; q++)   /* the standard idiom: stop at != end */
        count += (*q != 0);
    printf("non-zero elements: %d\n", count);
    return 0;
}
