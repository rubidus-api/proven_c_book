/* What a pointer actually points at - printed as addresses.
   The addresses themselves differ from run to run (ASLR); the *relations* never do:
   the stride, which one points where, and one level versus two. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 1. An array of pointers vs a 2-D array - same data, different layout */
static void two_shapes(void)
{
    /* array of pointers: three cells hold addresses; the text is scattered */
    const char *names[] = { "ada", "grace", "linus" };
    /* 2-D array: one block, every row the same width */
    char table[3][8] = { "ada", "grace", "linus" };

    printf("  names: sizeof = %zu (%zu cells x %zu)\n",
           sizeof names, sizeof names / sizeof names[0], sizeof names[0]);
    for (size_t i = 0; i < 3; i++)
        printf("    names[%zu] -> %p  \"%s\" (%zu bytes)\n",
               i, (const void *)names[i], names[i], strlen(names[i]) + 1);

    printf("  table: sizeof = %zu (%zu rows x %zu)\n",
           sizeof table, sizeof table / sizeof table[0], sizeof table[0]);
    for (size_t i = 0; i < 3; i++)
        printf("    &table[%zu] = %p  \"%s\"  (step %td)\n",
               i, (const void *)table[i], table[i],
               i == 0 ? (ptrdiff_t)0 : table[i] - table[i - 1]);
}

/* 2. A pointer to a pointer - where someone else's pointer gets changed */
static int take_buffer(char **out, size_t n)
{
    char *buf = malloc(n);
    if (buf == NULL)
        return -1;                 /* on failure, leave *out alone */
    memset(buf, 'x', n - 1);
    buf[n - 1] = '\0';
    *out = buf;                    /* <- writes into the caller's variable */
    return 0;
}

/* The same job with one level - only the copy changes, the original does not.
   (It is read once to avoid a set-but-unused warning; that the written value
    never leaves the function is precisely this function's point.) */
static void does_nothing(char *out, size_t n)
{
    char *buf = malloc(n);
    if (buf != NULL) {
        memset(buf, 'y', n - 1);
        buf[n - 1] = '\0';
        out = buf;                 /* this assignment ends inside this function */
        printf("    (inside: the local copy now points at %p)\n", (void *)out);
        free(buf);
    }
}

static void out_parameter(void)
{
    char *p = NULL;

    printf("  before the call: p = %p, and p lives at &p = %p\n",
           (void *)p, (void *)&p);
    does_nothing(p, 8);
    printf("  after passing one level:  p = %p (unchanged)\n", (void *)p);

    if (take_buffer(&p, 8) == 0) {
        printf("  after passing two levels: p = %p \"%s\"\n", (void *)p, p);
        free(p);
    }
}

/* 3. Sorting an array of pointers - the data stays, only the order changes */
static int by_text(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void sort_without_moving(void)
{
    const char *v[] = { "linus", "ada", "grace" };
    const void *before[3];

    for (size_t i = 0; i < 3; i++)
        before[i] = (const void *)v[i];
    qsort(v, 3, sizeof v[0], by_text);

    for (size_t i = 0; i < 3; i++) {
        int moved = 1;
        for (size_t j = 0; j < 3; j++)
            if (before[j] == (const void *)v[i])
                moved = 0;
        printf("    v[%zu] -> %p \"%s\"%s\n", i, (const void *)v[i], v[i],
               moved ? "  (a new address?!)" : "");
    }
    puts("    the three addresses are the original ones - not one byte of text moved.");
}

int main(void)
{
    printf("[an array of pointers vs a 2-D array]\n");
    two_shapes();

    printf("\n[a pointer to a pointer - changing the caller's variable]\n");
    out_parameter();

    printf("\n[sorting an array of pointers moves pointers, not data]\n");
    sort_without_moving();
    return 0;
}
