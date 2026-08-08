/* Why the sum of member sizes is not the struct's size — print the layout. */
#include <stddef.h>
#include <stdio.h>

/* the same members, only the order differs */
struct loose { char  a; int   b; char  c; };   /* a big one between small ones */
struct tight { int   b; char  a; char  c; };   /* biggest first */

int main(void)
{
    printf("sum of member sizes: %zu + %zu + %zu = %zu bytes\n",
           sizeof(char), sizeof(int), sizeof(char),
           sizeof(char) * 2 + sizeof(int));

    printf("\nstruct loose { char a; int b; char c; }\n");
    printf("  sizeof  = %zu, _Alignof = %zu\n",
           sizeof(struct loose), alignof(struct loose));
    printf("  offsetof(a) = %zu, offsetof(b) = %zu, offsetof(c) = %zu\n",
           offsetof(struct loose, a), offsetof(struct loose, b),
           offsetof(struct loose, c));

    printf("\nstruct tight { int b; char a; char c; }\n");
    printf("  sizeof  = %zu, _Alignof = %zu\n",
           sizeof(struct tight), alignof(struct tight));
    printf("  offsetof(b) = %zu, offsetof(a) = %zu, offsetof(c) = %zu\n",
           offsetof(struct tight, b), offsetof(struct tight, a),
           offsetof(struct tight, c));

    /* draw the layout: named cells for members, dots for the gaps */
    puts("\ncell by cell (numbers are offsets, dots are padding):");
    for (size_t i = 0; i < sizeof(struct loose); i++) {
        char mark = '.';
        if (i == offsetof(struct loose, a)) mark = 'a';
        else if (i >= offsetof(struct loose, b)
              && i <  offsetof(struct loose, b) + sizeof(int)) mark = 'b';
        else if (i == offsetof(struct loose, c)) mark = 'c';
        printf("%c", mark);
    }
    printf("   <- loose (%zu bytes)\n", sizeof(struct loose));
    for (size_t i = 0; i < sizeof(struct tight); i++) {
        char mark = '.';
        if (i < sizeof(int)) mark = 'b';
        else if (i == offsetof(struct tight, a)) mark = 'a';
        else if (i == offsetof(struct tight, c)) mark = 'c';
        printf("%c", mark);
    }
    printf("       <- tight (%zu bytes)\n", sizeof(struct tight));

    /* laid out as an array, the difference multiplies */
    printf("\nwith a million elements: loose %zu MiB, tight %zu MiB\n",
           sizeof(struct loose) * 1000000u / (1024 * 1024),
           sizeof(struct tight) * 1000000u / (1024 * 1024));
    return 0;
}
