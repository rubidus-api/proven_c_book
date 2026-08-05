/* The two ways of reading a declaration, confirmed with the eyes.
   Whether the type built up out of typedefs really is the original one is
   checked too. */
#include <stdio.h>

/* (1) the difference of one star ──────────────────────── */
int  *pa[3];      /* pa: array[3] of pointer to int   */
int (*ap)[3];     /* ap: pointer to array[3] of int   */

/* (2) when a function joins in ─────────────────────────── */
int  *f(void);    /* f: function(void) returning pointer to int */
int (*g)(void);   /* g: pointer to function(void) returning int */

/* (3) the notorious shape: array[4] of pointer to function(int) returning pointer to char */
char *(*table[4])(int);

/* (4) the same type, built one typedef layer at a time */
typedef char           *charptr;        /* pointer to char                  */
typedef charptr         handler(int);   /* function(int) returning charptr  */
typedef handler        *handler_ptr;    /* pointer to that function         */
typedef handler_ptr     table4[4];      /* array[4] of that pointer         */

/* whether the types built by the two roads really match, checked at compile time */
static_assert(sizeof(table4) == sizeof(table), "they must be the same type");

static char *shout(int n)  { (void)n; return "shout"; }
static char *quiet(int n)  { (void)n; return "quiet"; }

int main(void)
{
    printf("int  *pa[3]  : whole %zu, element %zu  -> %zu pointers\n",
           sizeof pa, sizeof pa[0], sizeof pa / sizeof pa[0]);
    printf("int (*ap)[3] : whole %zu, what it points to %zu\n",
           sizeof ap, sizeof *ap);

    /* (3) actually filled in and used */
    table[0] = shout;
    table[1] = quiet;
    printf("table[0](1) = %s, table[1](2) = %s\n", table[0](1), table[1](2));

    /* a variable made with (4)'s typedefs fits the same slot as it is */
    table4 other = { quiet, shout };
    printf("other[0](3) = %s   (the same type, built with typedefs)\n", other[0](3));

    /* the form without an identifier (an abstract declarator): used in casts and sizeof */
    printf("sizeof(char *(*)(int)) = %zu   (a nameless function pointer type)\n",
           sizeof(char *(*)(int)));
    return 0;
}
