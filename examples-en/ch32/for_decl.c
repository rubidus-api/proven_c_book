/* What may be declared in for's first slot (clause-1) - the measured version.
   Built as C23. What the comments say about C99 and C11 was checked with
   -std=c11 -pedantic-errors. */
#include <stdio.h>

/* 1. One declaration means one base type. Derived types may join it. */
static void one_base_type(void)
{
    int a[3] = { 10, 20, 30 };

    /* one int makes a value, two pointers and a count at once */
    for (int *p = a, *end = a + 3, n = 0; p != end; p++, n++)
        printf("  a[%d] = %d\n", n, *p);

    /* for (int i = 0; double x = 0.0;) is a syntax error -
       "expected expression before 'double'" on both gcc and clang */
}

/* 2. The name lives for one loop - the three slots and the body, and no further. */
static void scope_of_the_name(void)
{
    int i = 100;                        /* the outer i */

    for (int i = 0; i < 2; i++)         /* the inner i shadows the outer one */
        printf("  inside the loop i = %d\n", i);

    printf("  after the loop i = %d (the outer one was never touched)\n", i);
}

/* 3. C23's auto - the type is inferred from the initializer. */
static void c23_auto(void)
{
    for (auto i = 0; i < 3; i++)        /* i is inferred as int */
        printf("  auto i = %d\n", i);
}

/* 4. C23 dropped the storage-class constraint, so static is allowed here.
      Being allowed and being a good idea are two different things - this
      function is the reason. */
static void static_counter(void)
{
    for (static int calls = 0; calls < 2; calls++)
        printf("  static clause-1: this line ran (calls = %d)\n", calls);
}

int main(void)
{
    printf("[one declaration means one base type]\n");
    one_base_type();

    printf("\n[the name lives in the loop and nowhere else]\n");
    scope_of_the_name();

    printf("\n[C23: auto infers the type from the initializer]\n");
    c23_auto();

    printf("\n[C23: a static object in clause-1 is initialized once, ever]\n");
    printf("  first call:\n");
    static_counter();
    printf("  second call:\n");
    static_counter();
    printf("  third call:\n");
    static_counter();
    printf("  the second and third calls print nothing - the counter kept its\n");
    printf("  value from the first call, so the loop never ran again.\n");
    return 0;
}
