#include <stdio.h>

int main(void) {
    int a = 0, b = 0, k;
    double d = 0.0;
    char word[8] = {0};
    char rest[16] = {0};

    /* one space in the format means "any number of whitespace characters" */
    k = sscanf("  12   34", "%d %d", &a, &b);
    printf("ints    : k=%d a=%d b=%d\n", k, a, b);

    /* %s stops at whitespace. A width protects the buffer */
    k = sscanf("hello world", "%7s", word);
    printf("bounded : k=%d word=[%s]\n", k, word);

    /* an ordinary character in the format must match the input exactly */
    k = sscanf("x=5", "x=%d", &a);
    printf("literal : k=%d a=%d\n", k, a);

    /* if it does not match it is a matching failure — 0 is returned and the argument is untouched */
    k = sscanf("y=5", "x=%d", &a);
    printf("mismatch: k=%d (a stays %d)\n", k, a);

    /* a conversion failure is 0 as well — not a number, so nothing is read */
    k = sscanf("abc", "%d", &b);
    printf("nonnum  : k=%d (b stays %d)\n", k, b);

    /* an empty input gives EOF (a negative value) — it must be told apart from 0 */
    k = sscanf("", "%d", &b);
    printf("empty   : k=%d\n", k);

    /* partial success: the front is read and it stops further on */
    k = sscanf("7 oops", "%d %lf", &a, &d);
    printf("partial : k=%d a=%d\n", k, a);

    /* the set specifier: it gathers characters that are not a comma */
    k = sscanf("name,42", "%15[^,],%d", rest, &b);
    printf("set     : k=%d rest=[%s] b=%d\n", k, rest, b);

    /* reals and the length modifier: a double is %lf */
    k = sscanf("3.5", "%lf", &d);
    printf("double  : k=%d d=%.2f\n", k, d);
    return 0;
}
