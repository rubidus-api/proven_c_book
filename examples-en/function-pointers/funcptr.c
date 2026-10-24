#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }

/* a function taking a function pointer — the basic form of a callback */
static int apply(int (*op)(int, int), int a, int b) { return op(a, b); }

int main(void)
{
    /* (1) a function name decays to a pointer the moment it is used as a value */
    int (*p)(int, int) = add;     /* it works without & */
    int (*q)(int, int) = &add;    /* attaching & is the same */
    printf("add == &add : %s\n", p == q ? "same" : "different");

    /* (2) however many stars are attached the result is the same — the dereference decays again */
    printf("p(2,3)=%d  (*p)(2,3)=%d  (***p)(2,3)=%d  (*******p)(2,3)=%d\n",
           p(2, 3), (*p)(2, 3), (***p)(2, 3), (*******p)(2, 3));

    /* (3) & , on the other hand, can be used only once: &&add is a syntax error
          (&add is only a value, and the address of that value cannot be taken again) */

    /* (4) a dispatch table — choosing with an array instead of a switch */
    struct { const char *name; int (*fn)(int, int); } table[] = {
        { "add", add }, { "mul", mul },
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        printf("%s(6,7) = %d\n", table[i].name, apply(table[i].fn, 6, 7));

    /* (5) there is no guarantee that a function pointer is the size of a data pointer */
    printf("sizeof(void*)=%zu, sizeof(int(*)(int,int))=%zu\n",
           sizeof(void *), sizeof(int (*)(int, int)));

    /* (6) qsort's comparator is a function pointer too */
    int v[] = { 5, 2, 9, 1 };
    int cmp(const void *a, const void *b);   /* defined below */
    qsort(v, 4, sizeof v[0], cmp);
    printf("sorted: %d %d %d %d\n", v[0], v[1], v[2], v[3]);
    return 0;
}

int cmp(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}
