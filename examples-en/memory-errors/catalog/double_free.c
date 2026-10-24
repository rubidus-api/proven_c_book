/* Breaking the lifetime, freeing (1) --- return the same block twice. */
#include <stdlib.h>

[[gnu::noinline]] static void release(void *p) { free(p); }

int main(void)
{
    void *p = malloc(16);
    release(p);
    release(p);
}
