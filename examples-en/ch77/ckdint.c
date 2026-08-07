/* Overflow received not as a value but as "did it happen" — C23 <stdckdint.h> */
#include <limits.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The common allocation sum: n elements x size sz. If the multiplication overflows, the vessel comes out too small. */
static void *alloc_array(size_t n, size_t sz)
{
    size_t bytes;
    if (ckd_mul(&bytes, n, sz)) {       /* true means it overflowed */
        printf("  the size computation overflowed - nothing is allocated\n");
        return NULL;
    }
    return malloc(bytes);
}

int main(void)
{
    /* the test and the reading of the result are written apart (mixed in one expression there is no order) */
    int r;
    bool over = ckd_add(&r, INT_MAX, 1);
    printf("INT_MAX = %d\n", INT_MAX);
    printf("ckd_add(INT_MAX, 1)  overflow? %s   r = %d (the wrapped value)\n", over ? "yes" : "no", r);

    over = ckd_add(&r, 1, 2);
    printf("ckd_add(1, 2)        overflow? %s   r = %d\n", over ? "yes" : "no", r);

    /* even with mixed types the judgement is made on the mathematical value */
    signed char c;
    over = ckd_add(&c, 200, 100);
    printf("signed char <- 300   overflow? %s   c = %d\n", over ? "yes" : "no", c);

    /* unsigned subtraction: wrapping is defined behaviour, yet this still reports "it overflowed" */
    unsigned u;
    over = ckd_sub(&u, 3u, 5u);
    printf("unsigned  <- 3 - 5   overflow? %s   u = %u\n", over ? "yes" : "no", u);

    printf("\nallocation sums\n");
    void *ok = alloc_array(1000, sizeof(int));
    printf("  1000 x %zu -> %s\n", sizeof(int), ok ? "allocated" : "refused");
    free(ok);
    void *bad = alloc_array(SIZE_MAX / 2, sizeof(int));
    printf("  SIZE_MAX/2 x %zu -> %s\n", sizeof(int), bad ? "allocated" : "refused");
    free(bad);
    return 0;
}
