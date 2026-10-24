/* Addresses in a form people read — the contract of %p and the alternatives.
   * Addresses differ from run to run (ASLR), so this demonstration prints
     properties, not values: what is printed here must be the same every time. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    int n = 42;

    /* (1) %p takes a void* (and, from C23, a character-type pointer) only.
          Cast anything else — better to make it a habit. */
    puts("[the contract of %p]");
    printf("  a null pointer via %%p: %p   <- the form is implementation-defined\n",
           (void *)nullptr);
    puts("  Other addresses differ per run, so they are not printed here.");

    /* (2) print one address two ways and compare the strings */
    char a[64], b[64];
    snprintf(a, sizeof a, "%p", (void *)&n);
    snprintf(b, sizeof b, "%#" PRIxPTR, (uintptr_t)(void *)&n);
    printf("\n[do %%p and PRIxPTR produce the same text?]\n");
    printf("  on this implementation: %s\n", strcmp(a, b) == 0 ? "yes" : "no");
    printf("  same number of characters: %s (the value is not printed)\n",
           strlen(a) == strlen(b) ? "yes" : "no");

    /* (3) the guaranteed round trip is void* <-> uintptr_t */
    uintptr_t u = (uintptr_t)(void *)&n;
    printf("\n[the uintptr_t round trip]\n");
    printf("  is the pointer back equal to the original: %s\n",
           (void *)u == (void *)&n ? "yes — the standard promises it" : "no");

    /* (4) a fixed width keeps a log tidy (padded with zeroes) */
    char line[80];
    snprintf(line, sizeof line, "obj=0x%016" PRIxPTR, u);
    printf("\n[making one line of a log]\n");
    printf("  length of the line produced: %zu characters (constant, whatever the address)\n",
           strlen(line));

    /* (5) compare layout inside one array — subtracting across objects is outside the contract */
    int arr[3];
    printf("\n[comparing layout]\n");
    printf("  arr[0] to arr[1]: %td bytes (legal — the same array)\n",
           (char *)&arr[1] - (char *)&arr[0]);
    printf("  equal to sizeof(int): %s\n",
           (size_t)((char *)&arr[1] - (char *)&arr[0]) == sizeof(int) ? "yes" : "no");

    /* (6) a human-facing log wants names and ordinals, not addresses */
    puts("\n[names instead of addresses]");
    static const struct { const char *name; const void *at; } table[] = {
        { "n", nullptr }, { "arr", nullptr },
    };
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        printf("  #%zu %-4s <- this name is what the log keeps\n", i, table[i].name);
    puts("  An address means something only within that run; the next run differs.");
    return 0;
}
