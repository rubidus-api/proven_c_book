/* Missing the lifetime --- overwrite the last pointer and lose the first block. */
#include <stdlib.h>
#include <string.h>

[[gnu::noinline]] static char *copy(const char *s)
{
    char *d = malloc(strlen(s) + 1);
    if (d != NULL) strcpy(d, s);
    return d;
}

int main(void)
{
    char *a = copy("first");
    a = copy("second");     /* the only pointer to "first" is gone */
    free(a);
}
