#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 반례: 첫 글자만 비교한다. 타입은 완벽하게 맞고, 경고도 없다 */
static int cmp_first_char(const void *a, const void *b) {
    const char *const *x = a;
    const char *const *y = b;
    return (*x)[0] - (*y)[0];
}

/* 정례: 문자열 전체를 비교한다 */
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
