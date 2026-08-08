/* The same spelling x placed in all four name spaces at once — §6.2.3.
   It is legal and it compiles. Whether it reads well is another matter. */
#include <stdio.h>

/* (2) the tag name space — the name that follows struct/union/enum */
struct x {
    int x;        /* (3) the member name space — a yard of struct x's own */
    int y;
};

/* Another struct's members are another yard, so the spelling may recur */
struct point { int x; int y; };

/* (4) ordinary identifiers — variables, functions, typedef names and
      enumeration constants all live here. Same spelling as the tag x,
      different name space, so they coexist. */
typedef struct x x;

static int show(x v, struct point p)
{
    /* (1) the label name space — goto's target. This may be called x too */
    if (v.x < 0) goto x;

    printf("struct x    : x=%d y=%d\n", v.x, v.y);
    printf("struct point: x=%d y=%d\n", p.x, p.y);
    return 0;

x:  /* the label x */
    puts("negative, so we came to the label x");
    return 1;
}

int main(void)
{
    /* With one x in each of the four name spaces, use them all */
    x       a = { .x = 10, .y = 20 };   /* the typedef name x */
    struct x b = { .x = -1, .y = 0 };   /* the tag x */
    struct point p = { .x = 3, .y = 4 };

    puts("[the same spelling x lives in all four name spaces at once]");
    puts("  (1) label x  (2) tag struct x  (3) member x  (4) typedef name x");
    puts("");

    (void)show(a, p);
    (void)show(b, p);

    /* The syntactic context picks the name space:
       after struct -> tag, after . -> member, after goto -> label,
       anywhere else -> ordinary identifier. */
    printf("\nsizeof(x) = %zu, sizeof(struct x) = %zu  (the same type)\n",
           sizeof(x), sizeof(struct x));
    return 0;
}
