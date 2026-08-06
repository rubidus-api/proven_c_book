/* Why a macro's arguments must be wrapped in parentheses — the places where
   the intent and the expansion part ways. xstr() prints "what did it expand
   into, then" alongside, so it can be checked. */
#include <stdio.h>

#define str(...)   # __VA_ARGS__
#define xstr(...)  str(__VA_ARGS__)

#define SQ_BAD(x)   x * x           /* no parentheses */
#define SQ_HALF(x)  (x) * (x)       /* only the arguments wrapped */
#define SQ(x)       ((x) * (x))     /* arguments and the whole wrapped — the canonical form */

#define NEG_BAD(x)  -x
#define NEG(x)      (-(x))

/* a macro that uses an argument twice — when it meets an argument with a side effect */
#define MAX(a, b)   ((a) > (b) ? (a) : (b))

int main(void)
{
    /* ── (1) leave the argument unwrapped — precedence cuts in ────── */
    printf("SQ_BAD(1+2)  -> %-16s = %d   (9 was intended)\n",
           xstr(SQ_BAD(1+2)), SQ_BAD(1+2));
    printf("SQ_HALF(1+2) -> %-16s = %d\n",
           xstr(SQ_HALF(1+2)), SQ_HALF(1+2));

    /* ── (2) leave the whole unwrapped — the outer operator cuts in ─ */
    printf("100/SQ_HALF(2) -> %-14s = %d   (25 was intended)\n",
           xstr(100/SQ_HALF(2)), 100/SQ_HALF(2));
    printf("100/SQ(2)      -> %-14s = %d\n",
           xstr(100/SQ(2)), 100/SQ(2));

    /* ── (3) an argument carrying a sign ──────────────────────────── */
    printf("NEG_BAD(-3)  -> %-16s  <- a compile error (`--3`)\n", xstr(NEG_BAD(-3)));
    printf("NEG(-3)      -> %-16s = %d\n", xstr(NEG(-3)), NEG(-3));

    /* ── (4) what parentheses cannot stop — the argument is evaluated twice ─ */
    int i = 5, j = 3;
    int m = MAX(i++, j);
    printf("MAX(i++, j)  -> i has become %d (one increment was expected)\n", i);
    printf("             result m = %d\n", m);
    printf("             expansion: %s\n", xstr(MAX(i++, j)));
    return 0;
}
