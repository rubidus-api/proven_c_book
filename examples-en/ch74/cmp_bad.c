#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Counterexample: it compares only the first character. The types match perfectly and there is no warning */
static int cmp_first_char(const void *a, const void *b) {
    const char *const *x = a;
    const char *const *y = b;
    return (*x)[0] - (*y)[0];
}

/* The right version: it compares the whole string */
static int cmp_full(const void *a, const void *b) {
    const char *const *x = a;
    const char *const *y = b;
    return strcmp(*x, *y);
}

static void show(const char *label, const char **v, size_t n) {
    printf("%s:", label);
    for (size_t i = 0; i < n; i++) printf(" %s", v[i]);
    printf("\n");
}

int main(void) {
    const char *src[] = {"pear", "apple", "peach", "apricot"};
    const char *v[4];
    size_t n = sizeof src / sizeof src[0];

    memcpy(v, src, sizeof src);
    qsort(v, n, sizeof v[0], cmp_first_char);
    show("first-char", v, n);

    memcpy(v, src, sizeof src);
    qsort(v, n, sizeof v[0], cmp_full);
    show("full      ", v, n);
    return 0;
}
