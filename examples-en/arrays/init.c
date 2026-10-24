#include <stdio.h>

#define COUNT(a) (sizeof (a) / sizeof (a)[0])   /* real arrays only */

static void show(const char *label, const int *v, size_t n)
{
    printf("%-22s", label);
    for (size_t i = 0; i < n; i++)
        printf(" %d", v[i]);
    printf("   (%zu elements)\n", n);
}

static int counter[3];          /* static storage: 0 without an initializer */

int main(void)
{
    int full[5]    = {3, 1, 4, 1, 5};   /* all five slots written */
    int part[5]    = {3, 1};            /* the other three are 0 */
    int sized[]    = {2, 7, 1, 8};      /* size taken from the initializer */
    int marked[6]  = {[4] = 9, [1] = 2};/* designated initializers (C99) */
    int empty[4]   = {};                /* C23: empty braces, all zero */

    puts("[1] initializers");
    show("{3, 1, 4, 1, 5}", full, COUNT(full));
    show("{3, 1}", part, COUNT(part));
    show("[] = {2, 7, 1, 8}", sized, COUNT(sized));
    show("{[4] = 9, [1] = 2}", marked, COUNT(marked));
    show("{}", empty, COUNT(empty));
    show("static, no initializer", counter, COUNT(counter));

    puts("\n[2] the array type knows its size");
    printf("sizeof full    = %zu bytes\n", sizeof full);
    printf("sizeof full[0] = %zu bytes\n", sizeof full[0]);
    printf("COUNT(full)    = %zu\n", COUNT(full));

    puts("\n[3] no whole-array assignment: copy element by element");
    int copy[5];
    for (size_t i = 0; i < COUNT(full); i++)
        copy[i] = full[i];                  /* copy = full; is an error */
    show("copy of full", copy, COUNT(copy));

    puts("\n[4] the first element shares the array's address");
    printf("(void *)full == (void *)&full[0] : %d\n",
           (void *)full == (void *)&full[0]);
    return 0;
}
