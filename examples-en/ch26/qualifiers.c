/* See all four type qualifiers in one place.
   The point: a qualified type is another edition of the same type - neither
   its size nor its alignment changes. */
#include <stdio.h>
#include <stdalign.h>

struct big { char a[9]; };

int main(void)
{
    puts("(1) a qualifier changes neither size nor alignment");
    printf("  int            : %zu / %zu\n", sizeof(int), alignof(int));
    printf("  const int      : %zu / %zu\n", sizeof(const int), alignof(const int));
    printf("  volatile int   : %zu / %zu\n", sizeof(volatile int), alignof(volatile int));
    printf("  struct big     : %zu / %zu\n", sizeof(struct big), alignof(struct big));
    printf("  _Atomic big    : %zu / %zu   <- unchanged on this implementation\n",
           sizeof(_Atomic struct big), alignof(_Atomic struct big));

    puts("\n(2) the order is free and they may be combined");
    const volatile int a = 1;
    volatile const int b = 2;      /* exactly the same type as above */
    printf("  const volatile int a = %d,  volatile const int b = %d\n", a, b);

    puts("\n(3) _Atomic has two faces");
    _Atomic int   c = 3;           /* written as a qualifier */
    _Atomic(int)  d = 4;           /* written as a type specifier - same type */
    printf("  _Atomic int c = %d,  _Atomic(int) d = %d\n", c, d);
    /* typedef int A[3];  _Atomic A x;   <- cannot qualify an array: compile error */

    puts("\n(4) const only says \"not through this name\"");
    int  x  = 10;
    const int *p = &x;             /* cannot change it through p */
    x = 20;                        /* but x itself still can */
    printf("  changed x directly, so *p = %d  <- const is a promise about the route, not the value\n", *p);
    /* *p = 30;  <- error: assignment of read-only location */

    puts("\n(5) a qualifier may be added silently, never removed");
    const int ci = 7;
    const int *q = &ci;            /* adding: allowed */
    printf("  int * -> const int * passes; the reverse warns (-Wdiscarded-qualifiers), *q = %d\n", *q);
    /* int *bad = &ci;  <- warning: initialization discards 'const' qualifier */

    return 0;
}
