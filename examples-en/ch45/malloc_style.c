/* Three ways of writing a malloc call — how they differ. */
#include <stdio.h>
#include <stdlib.h>

struct node { int id; double weight; };

int main(void)
{
    /* -- the recommended form: the type name never appears -- */
    struct node *a = malloc(4 * sizeof *a);
    if (!a) return 1;

    /* -- the C++-compatible form: the type name twice -- */
    struct node *b = (struct node *)malloc(4 * sizeof(struct node));
    if (!b) { free(a); return 1; }

    /* -- the compromise: cast, but take the size from the target -- */
    struct node *c = (struct node *)malloc(4 * sizeof *c);
    if (!c) { free(a); free(b); return 1; }

    printf("all three reserve the same size: %zu, %zu, %zu bytes\n",
           4 * sizeof *a, 4 * sizeof(struct node), 4 * sizeof *c);

    puts("\n[sizeof *p does not read p — an unevaluated operand]");
    struct node *nul = NULL;
    printf("  even with p null, sizeof *p computes as %zu\n", sizeof *nul);
    puts("  because the size comes from the TYPE, not from the value.");

    puts("\n[write the name twice and the two can drift apart]");
    puts("  struct node *p = (struct node *)malloc(n * sizeof(struct gadget));");
    puts("  ^ the type name is in two places, so fixing only one is possible.");
    puts("  written as sizeof *p, that accident CANNOT BE WRITTEN.");

    puts("\n[a danger both forms share — overflow in the multiplication]");
    size_t huge = (size_t)-1 / sizeof(struct node) + 1;
    printf("  huge * sizeof *a wraps around to %zu\n", (size_t)(huge * sizeof *a));
    puts("  -> malloc can SUCCEED with a tiny size. Check the count first, or"); 
    puts("     use calloc(count, size), which checks the multiplication itself.");

    free(a); free(b); free(c);
    return 0;
}
