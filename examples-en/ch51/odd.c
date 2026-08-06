/* The odd examples the standard itself gives — the expansions printed as text.
   Wrap a thing in xstr() and "what did it become, then" can be seen as a
   string (the double indirection). */
#include <stdio.h>

/* to stringize even something containing commas, take it as variadic arguments */
#define str(...)   # __VA_ARGS__
#define xstr(...)  str(__VA_ARGS__)

/* ── (1) placemarkers — pasting an empty argument with ## ──────── */
#define t(x, y, z)  x ## y ## z

int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),
            t(10,,), t(,11,), t(,,12), t(,,) };

/* ── (2) the expansion the standard pins down as "unspecified" ─── */
#define f(a)  a*g
#define g(a)  f(a)
int g = 1;                     /* g is a function-like macro, so used alone it is not expanded */

/* ── (3) __VA_OPT__ — when is an argument judged "empty"? ──────── */
#define LOG(...)          log(0 __VA_OPT__(,) __VA_ARGS__)
#define SDEF(name, ...)   S name __VA_OPT__(= { __VA_ARGS__ })
#define EMP

int main(void)
{
    printf("(1) placemarkers\n");
    printf("   j[] = {");
    for (size_t i = 0; i < sizeof j / sizeof *j; i++)
        printf(" %d", j[i]);
    printf(" }   (%zu elements)\n", sizeof j / sizeof *j);
    printf("   t(,,)  -> \"%s\"  (not one token is left)\n", xstr(t(,,)));
    printf("   t(6,,7)-> \"%s\"\n", xstr(t(6,,7)));

    printf("\n(2) unspecified expansion\n");
    printf("   f(2)(9) -> \"%s\"\n", xstr(f(2)(9)));
    printf("   (the standard does not settle which of \"2*9*g\" and \"2*9*f(9)\" it is)\n");

    printf("\n(3) __VA_OPT__\n");
    printf("   LOG(1,2)  -> \"%s\"\n", xstr(LOG(1,2)));
    printf("   LOG()     -> \"%s\"\n", xstr(LOG()));
    printf("   LOG(EMP)  -> \"%s\"   <- an argument was passed, yet there is no comma\n", xstr(LOG(EMP)));
    printf("   SDEF(foo)      -> \"%s\"\n", xstr(SDEF(foo)));
    printf("   SDEF(bar,1,2)  -> \"%s\"\n", xstr(SDEF(bar, 1, 2)));
    return 0;
}
