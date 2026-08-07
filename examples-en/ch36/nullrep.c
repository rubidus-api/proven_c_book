/* The 0 in the source and the bits in memory — spelling and representation. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Look straight at the bytes of one pointer */
static void dump(const char *how, const void *pobj, size_t n)
{
    const unsigned char *b = pobj;
    int all_zero = 1;

    printf("  %-22s", how);
    for (size_t i = 0; i < n; i++) {
        printf(" %02X", b[i]);
        if (b[i] != 0) all_zero = 0;
    }
    printf("   all bits zero? %s\n", all_zero ? "yes" : "no");
}

struct node {
    int          id;
    struct node *next;      /* a pointer member */
    double       weight;    /* a floating member */
};

int main(void)
{
    printf("pointer size on this implementation: %zu bytes\n\n", sizeof(int *));

    /* (1) all three spellings are null pointer constants — the syntax layer */
    puts("[the same null, written three ways]");
    int *a = 0;          dump("int *a = 0;",       &a, sizeof a);
    int *b = NULL;       dump("int *b = NULL;",    &b, sizeof b);
    int *c = nullptr;    dump("int *c = nullptr;", &c, sizeof c);
    printf("  are the three equal? %s\n",
           (a == b && b == c) ? "yes — the standard promises it" : "no");

    /* 0L and (void *)0 are null pointer constants too. A variable holding 0
       is not:
         const int zero = 0;
         int *p = zero;        <- a compile error: a variable, not a constant. */
    int *d = 0L;         dump("int *d = 0L;",        &d, sizeof d);
    int *e = (void *)0;  dump("int *e = (void *)0;", &e, sizeof e);

    /* (2) comparison and assignment always hold, whatever the bits are */
    puts("\n[comparison holds regardless of representation]");
    printf("  a == 0      : %s\n", a == 0       ? "true" : "false");
    printf("  a == NULL   : %s\n", a == NULL    ? "true" : "false");
    printf("  a == nullptr: %s\n", a == nullptr ? "true" : "false");
    printf("  !a          : %s\n", !a           ? "true" : "false");

    /* (3) two ways of filling a struct with zero — they mean different things */
    puts("\n[two ways of emptying a struct]");
    struct node x = { 0 };              /* the value layer: null and 0.0 promised */
    struct node y;
    memset(&y, 0, sizeof y);            /* the representation layer: all bits zero */

    dump("next after { 0 }", &x.next, sizeof x.next);
    dump("next after memset", &y.next, sizeof y.next);
    printf("  x.next == nullptr : %s   <- the standard promises it\n",
           x.next == nullptr ? "true" : "false");
    printf("  x.weight == 0.0   : %s   <- the standard promises it\n",
           x.weight == 0.0 ? "true" : "false");
    printf("  y.next == nullptr : %s   <- true only on this implementation\n",
           y.next == nullptr ? "true" : "false");

    /* (4) calloc belongs to the all-bits-zero side too */
    puts("\n[what calloc gives is zeroed bits]");
    int **arr = calloc(4, sizeof *arr);
    if (!arr) { perror("calloc"); return 1; }
    dump("arr[0] from calloc", &arr[0], sizeof arr[0]);
    printf("  arr[0] == nullptr : %s   <- true only on this implementation\n",
           arr[0] == nullptr ? "true" : "false");
    free(arr);

    puts("\nIn short: the 0 in the source is a *spelling*; the bits are a");
    puts("      *representation*. Comparison and assignment live in the first");
    puts("      layer and always hold; memset and calloc live in the second");
    puts("      and promise nothing about null.");
    return 0;
}
