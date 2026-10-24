/* The condition slot is not a test but code that runs every turn - measured. */
#include <stdio.h>

static int limit_calls = 0;

/* called on every turn, counting how often it was called */
static int limit(void)
{
    limit_calls++;
    return 5;
}

static int probe_calls = 0;

static int probe(int i)
{
    probe_calls++;
    return i < 3;
}

int main(void)
{
    printf("[the condition runs once more than the body]\n");
    int turns = 0;
    for (int i = 0; i < limit(); i++)
        turns++;
    printf("  body ran %d times, limit() was called %d times\n", turns, limit_calls);
    printf("  the last call is the one that fails and ends the loop\n");

    printf("\n[short-circuit can skip the side effect entirely]\n");
    probe_calls = 0;
    int guard = 0;                       /* if the left side is false the right side is not evaluated */
    for (int i = 0; guard && probe(i); i++)
        ;
    printf("  guard is false: probe() was called %d time(s)\n", probe_calls);
    probe_calls = 0;
    guard = 1;
    turns = 0;
    for (int i = 0; guard && probe(i); i++)
        turns++;
    printf("  guard is true : probe() was called %d time(s), body ran %d times\n",
           probe_calls, turns);

    printf("\n[assignment inside the condition - the parentheses are the point]\n");
    const char *text = "abc";
    const char *p = text;
    int c = 0, seen = 0, first = 0;
    while ((c = *p++) != '\0') {         /* store first, then compare */
        if (seen == 0)
            first = c;
        seen++;
    }
    printf("  with parentheses   : read %d characters, first value stored = %d ('%c')\n",
           seen, first, (char)first);

    p = text;
    seen = 0;
    first = 0;
    while ((c = (*p++ != '\0'))) {       /* comparing first stores only 0 or 1 */
        if (seen == 0)
            first = c;
        seen++;
    }
    printf("  without parentheses: read %d characters, first value stored = %d\n",
           seen, first);
    printf("  the character is gone - only the truth value was kept\n");
    printf("  gcc says: suggest parentheses around assignment used as truth value\n");

    printf("\n[changing the counter inside the condition]\n");
    int n = 3;
    printf("  while (i < n)  visits:");
    for (int i = 0; i < n; i++)
        printf(" %d", i);
    printf("\n  while (i++ < n) visits:");
    int i = 0;
    while (i++ < n)
        printf(" %d", i);
    printf("\n  while (++i < n) visits:");
    i = 0;
    while (++i < n)
        printf(" %d", i);
    printf("\n  i++ added 1 before the body saw i: 1..3 instead of 0..2,\n");
    printf("  and ++i also compares the new value, so it turns one time fewer\n");
    return 0;
}
