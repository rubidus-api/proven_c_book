/* Breaking the lifetime, reading --- read a block that was already returned. */
#include <stdlib.h>

[[gnu::noinline]] static int peek(const int *p) { return *p; }

int main(void)
{
    int *p = malloc(sizeof *p);
    if (p == NULL) return 1;
    *p = 7;
    free(p);
    return peek(p);         /* the value of p is now indeterminate (C23 6.2.4p2) */
}
