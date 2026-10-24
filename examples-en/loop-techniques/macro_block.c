/* do { } while (0), which makes a macro one statement — and cleanup by goto. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* With braces alone, the trailing semicolon makes it two statements,
   so putting it between if and else is a compile error:

       #define SWAP_BAD(a, b) { int t = (a); (a) = (b); (b) = t; }
       if (x < y) SWAP_BAD(x, y); else puts("...");
           → error: 'else' without a previous 'if'

   do { } while (0) makes it a braced block and a single statement at once. */
#define SWAP(a, b)  do { int t_ = (a); (a) = (b); (b) = t_; } while (0)

#define LOG(fmt, ...)  do {                       \
        printf("[log] " fmt "\n", __VA_ARGS__);   \
    } while (0)

/* Gathering the cleanup in one place with goto — the Linux kernel's shape.
   Fail midway and only what was acquired so far is given back. */
static bool build(size_t n, bool fail_at_second)
{
    char *a = nullptr, *b = nullptr;
    bool ok = false;

    a = malloc(n);
    if (!a) goto out;                 /* nothing to give back yet */
    memset(a, 'a', n);

    b = fail_at_second ? nullptr : malloc(n);
    if (!b) goto free_a;              /* give back only a */
    memset(b, 'b', n);

    ok = true;

    free(b);
free_a:
    free(a);
out:
    return ok;
}

int main(void)
{
    int x = 1, y = 2;

    puts("[do { } while (0) — the macro becomes one statement]");
    if (x < y) SWAP(x, y); else puts("  (we do not come here)");
    printf("  after the swap: x=%d y=%d   <- unbroken between if and else\n", x, y);

    LOG("values %d and %d", x, y);

    puts("\n[safe as a for body without braces, too]");
    for (int i = 0; i < 2; i++)
        LOG("iteration %d", i);

    puts("\n[gathering cleanup with goto]");
    printf("  both succeed: %s\n", build(16, false) ? "ok" : "failed");
    printf("  second one fails: %s  <- only the first was given back\n",
           build(16, true) ? "ok" : "failed");
    return 0;
}
