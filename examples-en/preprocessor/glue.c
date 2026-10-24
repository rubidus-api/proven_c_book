/* Double expansion — the example the C standard gives, run as it stands.
   How one layer and two layers part ways is all here. */
#include <stdio.h>

/* ── the macros of the standard's example (C17 6.10.3.5) ──────── */
#define str(s)       # s              /* stringize */
#define xstr(s)      str(s)           /* the version wrapped one layer more */
#define INCFILE(n)   vers ## n        /* token pasting */
#define glue(a, b)   a ## b
#define xglue(a, b)  glue(a, b)       /* the version wrapped one layer more */
#define HIGHLOW      "hello"
#define LOW          LOW ", world"    /* it calls itself — the stage for the blue paint rule */

/* the assembled name becomes the header name: xstr(INCFILE(2).h) -> "vers2.h" */
#include xstr(INCFILE(2).h)

#define WIDTH 80

int main(void)
{
    /* ── (1) stringizing: one layer does not expand, two layers do ── */
    printf("str(WIDTH)   = %s\n", str(WIDTH));    /* the argument is the operand of # */
    printf("xstr(WIDTH)  = %s\n", xstr(WIDTH));   /* expanded first, then turned into a string */

    /* ── (2) pasting: those two lines of the standard's example ───── */
    printf("glue(HIGH, LOW)  = %s\n", glue(HIGH, LOW));
    printf("xglue(HIGH, LOW) = %s\n", xglue(HIGH, LOW));

    /* ── (3) did the header included under the assembled name arrive? ── */
    printf("VERS_TAG = %s\n", VERS_TAG);

    /* ── (4) the standard example's two stringizings ──────────────── */
    printf("%s\n", str(strncmp("abc\0d", "abc", '\4') == 0));
    return 0;
}
