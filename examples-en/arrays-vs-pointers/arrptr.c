/* Split array and pointer apart in one place.
   The point is how two values naming the same address behave differently. */
#include <stdio.h>

static int  arr[5] = { 10, 20, 30, 40, 50 };
static int *ptr    = arr;          /* holds the decayed address */

static void by_param(int p[5])     /* writing [5] does not stop it being a pointer */
{
    /* Today's compiler warns right here (-Wsizeof-array-argument).
       The warning is right - we mute just that one to show the effect. */
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsizeof-array-argument"
#endif
    printf("  inside the function, sizeof p = %zu  (one pointer)\n", sizeof p);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
}

int main(void)
{
    puts("(1) What does it hold?  The size gives it away");
    printf("  sizeof arr = %zu   sizeof ptr = %zu\n", sizeof arr, sizeof ptr);
    printf("  element count: sizeof arr / sizeof arr[0] = %zu\n",
           sizeof arr / sizeof arr[0]);

    puts("\n(2) Same address, different type - +1 steps by a different width");
    printf("  arr      = %p\n", (void *)arr);
    printf("  &arr[0]  = %p   (the same)\n", (void *)&arr[0]);
    printf("  &arr     = %p   (the same - but its type is int(*)[5])\n", (void *)&arr);
    printf("  arr  + 1 = %p   (+%td bytes)\n",
           (void *)(arr + 1), (char *)(arr + 1) - (char *)arr);
    printf("  &arr + 1 = %p   (+%td bytes - it steps over the whole array)\n",
           (void *)(&arr + 1), (char *)(&arr + 1) - (char *)arr);

    puts("\n(3) Which one can move");
    ptr = ptr + 2;                 /* a pointer can move */
    printf("  moving ptr two cells gives *ptr = %d\n", *ptr);
    /* arr = ptr;  <- an array name is not a modifiable lvalue: compile error */
    ptr = arr;

    puts("\n(4) A subscript is a pointer offset - so it may be reversed");
    printf("  arr[3] = %d,  *(arr + 3) = %d,  3[arr] = %d\n",
           arr[3], *(arr + 3), 3[arr]);

    puts("\n(5) Pass it to a function and the size disappears");
    printf("  before the call, sizeof arr   = %zu\n", sizeof arr);
    by_param(arr);

    return 0;
}
