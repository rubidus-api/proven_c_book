/* 수명을 놓친 것 --- 마지막 포인터를 덮어써서 첫 블록을 잃는다. */
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
    a = copy("second");     /* "first" 를 가리키던 유일한 포인터가 사라졌다 */
    free(a);
}
