/* See the precedence rule of boustrophedonic reading with your own eyes.
   In particular, split apart whether const/volatile attaches to the asterisk
   on its left or to the type. */
#include <stdio.h>

static int one = 1, two = 2;

int main(void)
{
    /* (1) const sits next to the type specifier -> it applies to the type */
    const int *pci  = &one;     /* pointer to int const */
    int const *pci2 = &one;     /* exactly the same - only the order differs */

    /* (2) const is not next to a type -> it applies to the asterisk on its left */
    int *const cpi = &one;      /* read-only pointer to int */

    /* (3) both */
    const int *const cpci = &one;

    /* (1) can be repointed.  *pci = 9; is an error. */
    pci = &two;
    pci2 = &two;
    printf("(1) *pci=%d  *pci2=%d   <- the pointer itself can move\n", *pci, *pci2);

    /* (2) is the reverse.  cpi = &two; is an error; the value can change. */
    *cpi = 42;
    printf("(2) *cpi=%d   one=%d      <- the value can change\n", *cpi, one);

    printf("(3) *cpci=%d              <- both are read-only\n", *cpci);

    /* (4) read boustrophedonically: right first, left when blocked */
    char *const *next = NULL;   /* next is a pointer to a read-only
                                   pointer to char */
    printf("(4) sizeof next = %zu   (one pointer)\n", sizeof next);

    /* (5) the tag is what lets it point at itself */
    struct node_tag { int datum; struct node_tag *next; };
    struct node_tag b = { 2, NULL };
    struct node_tag a = { 1, &b };
    printf("(5) a.datum=%d -> a.next->datum=%d\n", a.datum, a.next->datum);

    return 0;
}
