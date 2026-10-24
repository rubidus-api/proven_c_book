/* Breaking the size, writing (2) --- 8 bytes borrowed from the heap, written at the ninth place. */
#include <stdlib.h>

[[gnu::noinline]] static void put(char *p, size_t i) { p[i] = 'x'; }

int main(void)
{
    char *p = malloc(8);
    if (p == NULL) return 1;
    put(p, 8);
    free(p);
}
