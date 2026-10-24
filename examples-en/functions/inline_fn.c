/* The function specifier inline - what it promises and what it does not.
   Only the two forms that work in practice; the rest is left in comments. */
#include <stdio.h>

/* (1) A helper used inside one file - the safest form.
      Being static it is this translation unit's definition; no external
      definition is needed elsewhere. */
static inline int square(int x) { return x * x; }

/* (2) The form for a header used by several files.
      The two lines below are a pair - one inline definition plus one extern
      declaration. The extern declaration says "this translation unit emits
      the external definition". */
inline int cube(int x) { return x * x * x; }
extern int cube(int x);

int main(void)
{
    printf("(1) static inline square(5) = %d\n", square(5));
    printf("(2) inline + extern  cube(3) = %d\n", cube(3));

    /* (3) Its address can be taken - a function is a function.
          But the moment you take it, one real definition must exist. */
    int (*f)(int) = square;
    int (*g)(int) = cube;
    printf("(3) calling through pointers: f(6)=%d  g(2)=%d\n", f(6), g(2));

    /* (4) inline is a request, not a promise. The compiler may expand it
          or may not. */
    printf("(4) the answer is the same, expanded or not: %d %d\n", square(4), f(4));

    return 0;
}
