/* The two things assignment does — yielding a value, and changing an object. */
#include <stdio.h>

static int calls;

static int where(void)      /* the computation that picks the place is evaluated too */
{
    calls++;
    printf("  where() called — it picks the place on the left\n");
    return 1;
}

static int what(void)
{
    calls++;
    printf("  what() called — it makes the value on the right\n");
    return 42;
}

int main(void)
{
    int a[3] = {0, 0, 0};

    /* ── (1) assignment is an expression — it yields a value ─────── */
    int x, y;
    x = (y = 7) + 1;                  /* the value of the expression y = 7 is 7 */
    printf("x = (y = 7) + 1  ->  x=%d y=%d\n", x, y);

    int p, q, r;
    p = q = r = 5;                    /* right-associative — r is filled first */
    printf("p = q = r = 5    ->  p=%d q=%d r=%d\n", p, q, r);

    /* ── (2) the left side is evaluated too ──────────────────────── */
    puts("\nrunning a[where()] = what();");
    calls = 0;
    a[where()] = what();
    printf("  both functions were called (%d calls), a[1]=%d\n", calls, a[1]);
    puts("  which of the two is called first is not settled by the standard (unspecified).");

    /* ── (3) compound assignment evaluates the left side once ────── */
    puts("\nrunning a[where()] += 1;");
    calls = 0;
    a[where()] += 1;
    printf("  where() call count: %d (spelled out as a[where()] = a[where()] + 1 it would be 2)\n",
           calls);
    printf("  a[1] = %d\n", a[1]);

    /* ── (4) assignment yields the value *converted* to the left type ─ */
    char c;
    int wide = 321;
    int back = (c = (char)wide);      /* narrowed then widened is not the original */
    printf("\nchar c = (char)321 -> c=%d, value of (c = ...) = %d\n", c, back);

    double d;
    int truncated = (int)(d = 3.9);   /* real to integer truncates toward zero */
    printf("d = 3.9 -> d=%.1f, (int)d = %d\n", d, truncated);

    /* ── (5) side effects get their own statements ──────────────── */
    int i = 0;
    a[i] = 10;
    i++;                              /* never mixed into one expression */
    a[i] = 20;
    printf("\nthe safe shape: a[0]=%d a[1]=%d i=%d\n", a[0], a[1], i);
    return 0;
}
