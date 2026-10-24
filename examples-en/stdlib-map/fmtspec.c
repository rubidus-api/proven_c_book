#include <stdio.h>

int main(void) {
    int n = 42;
    unsigned u = 255;
    double x = 3.14159265;
    const char *s = "proven";
    size_t sz = 1234;
    long long big = 9000000000LL;

    /* flags and width: right alignment by default, - for left, 0 to fill with zeros */
    printf("[%d] [%6d] [%-6d] [%06d] [%+d]\n", n, n, n, n, n);

    /* precision: decimal places for reals, a maximum length for strings */
    printf("[%f] [%.2f] [%10.3f] [%e] [%g]\n", x, x, x, x, x);
    printf("[%s] [%10s] [%-10s] [%.3s]\n", s, s, s, s);

    /* bases and characters */
    printf("[%c] [%x] [%X] [%#x] [%o] [%u]\n", 'A', u, u, u, u, u);

    /* length modifiers: they tell the format the width of the argument */
    printf("[%zu] [%lld]\n", sz, big);

    /* passing the width and precision as arguments */
    printf("[%.*s] [%*d]\n", 4, s, 6, n);

    /* the percent character itself */
    printf("100%%\n");
    return 0;
}
