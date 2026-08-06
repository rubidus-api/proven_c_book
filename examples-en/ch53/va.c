#include <stdarg.h>
#include <stdio.h>

/* taking the count as the first argument — it has no way of knowing the count itself */
int sum_n(int count, ...)
{
    va_list ap;
    int total = 0;

    va_start(ap, count);            /* read from after count */
    for (int i = 0; i < count; i += 1) {
        total += va_arg(ap, int);   /* *we* are the ones telling it the type */
    }
    va_end(ap);                     /* it must be closed */
    return total;
}

/* using a sentinel in place of a count */
int sum_until_zero(int first, ...)
{
    va_list ap;
    int total = first;

    va_start(ap, first);
    for (;;) {
        int v = va_arg(ap, int);
        if (v == 0) { break; }
        total += v;
    }
    va_end(ap);
    return total;
}

int main(void)
{
    printf("sum_n(4, 1,2,3,4)        = %d\n", sum_n(4, 1, 2, 3, 4));
    printf("sum_until_zero(1,2,3,0)  = %d\n", sum_until_zero(1, 2, 3, 0));
    return 0;
}
