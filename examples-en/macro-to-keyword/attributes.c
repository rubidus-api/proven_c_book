/* C23 attributes - writing down intent the type system cannot carry */
#include <stdio.h>
#include <stdlib.h>

/* (1) nodiscard - a function whose result must not be thrown away.
   Put it on a function that reports failure by value and forgetting the
   check becomes a warning. */
[[nodiscard]] static int checked_add(int a, int b, int *out)
{
    if (b > 0 && a > 2147483647 - b) return 0;   /* overflow: failure */
    *out = a + b;
    return 1;
}

/* (2) maybe_unused - for things some build configurations do not use */
static void log_line([[maybe_unused]] const char *tag, const char *msg)
{
#ifdef VERBOSE
    printf("  [%s] %s\n", tag, msg);
#else
    printf("  %s\n", msg);
#endif
}

/* (3) noreturn - this one never comes back; the caller's next line is dead */
[[noreturn]] static void die(const char *why)
{
    printf("  fatal: %s\n", why);
    exit(EXIT_FAILURE);
}

static const char *classify(int n)
{
    switch (n) {
    case 0:
        printf("  zero, and it keeps going\n");
        [[fallthrough]];          /* (4) falling through on purpose */
    case 1:
        return "small";
    default:
        return "large";
    }
}

int main(void)
{
    int sum = 0;

    if (checked_add(2000000000, 2000000000, &sum))
        printf("  sum: %d\n", sum);
    else
        printf("  overflow refused\n");

    if (checked_add(2, 3, &sum))
        printf("  sum: %d\n", sum);

    log_line("main", "attributes carry intent, not types");
    printf("  classify(0) = %s\n", classify(0));
    printf("  classify(9) = %s\n", classify(9));

    if (sum != 5) die("checked_add lost a value");
    return 0;
}
