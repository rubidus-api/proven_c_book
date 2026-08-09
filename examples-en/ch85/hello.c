#include <proven.h>

int main(void)
{
    const char *name = "world";
    int         count = 3;
    double      ratio = 0.75;
    bool        ready = true;
    char        grade = 'A';

    /* {} is only a placeholder and has no type. The type comes from the argument */
    proven_println("Hello, {}! You have {} messages.",
                   PROVEN_ARG(name), PROVEN_ARG(count));

    /* whatever the type, it goes through the same placeholder */
    proven_println("ratio={} ready={} grade={}",
                   PROVEN_ARG(ratio), PROVEN_ARG(ready), PROVEN_ARG(grade));

    /* the format specification goes after the colon — width, alignment, digits */
    proven_println("|{:>8}|{:<8}|{:.3}|",
                   PROVEN_ARG(name), PROVEN_ARG(name), PROVEN_ARG(ratio));
    return 0;
}
