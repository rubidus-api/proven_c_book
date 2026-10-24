/* Strict aliasing --- what happens when an int lvalue and a float lvalue designate the same place.
 * The two parameters of store_both have *different types*. The compiler may read this as "an int object
 * and a float object do not overlap" (C23 6.5.1p7) --- and on that premise not read *ip again. */
#include <stdio.h>

[[gnu::noinline]] static int store_both(int *ip, float *fp)
{
    *ip = 1;
    *fp = 0.0f;
    return *ip;             /* compiler: "a float store cannot change an int -> 1" */
}

int main(void)
{
    int x = 42;
    int r = store_both(&x, (float *)&x);    /* outside the contract: writes an int object as a float */
    printf("returned %d, x is now %d\n", r, x);
    return 0;
}
