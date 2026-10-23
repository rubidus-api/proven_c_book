#include <stdio.h>

static int calls;

static bool hit(bool v)          /* changes calls: a side effect */
{
    calls++;
    return v;
}

int main(void)
{
    puts("[1] an expression has a value, and may also do something");
    int x = 0;
    int v = (x = 5) + 1;        /* the assignment has value 5, and x changes */
    printf("v = %d, x = %d\n", v, x);

    int i = 3;
    int post = i++;             /* value 3, effect: i becomes 4 */
    int pre  = ++i;             /* value 5, effect: i becomes 5 */
    printf("post = %d, pre = %d, i = %d\n", post, pre, i);

    puts("\n[2] && skips its right side --- and the side effects in it");
    calls = 0;
    bool r = hit(false) && hit(true);
    printf("result %d, hit() called %d time(s)\n", r, calls);
    calls = 0;
    r = hit(true) || hit(false);
    printf("result %d, hit() called %d time(s)\n", r, calls);

    puts("\n[3] the comma operator: left, then right; value of the right");
    int c = (calls = 0, calls + 10);
    printf("c = %d\n", c);

    puts("\n[4] one change per object between sequence points");
    int k = 0;
    k++, k++;                   /* comma operator: sequence point */
    k++;                        /* each full expression ends one */
    k++;
    printf("k = %d\n", k);
    return 0;
}
