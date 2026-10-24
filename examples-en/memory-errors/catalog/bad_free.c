/* Breaking the lifetime, freeing (2) --- return an address never given out (the middle of a block). */
#include <stdlib.h>

[[gnu::noinline]] static void release(void *p) { free(p); }

int main(void)
{
    char *p = malloc(16);
    if (p == NULL) return 1;
    release(p + 4);
}
