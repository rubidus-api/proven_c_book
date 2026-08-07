#include <stdio.h>

int next_ticket(void)
{
    static int issued = 0;      /* static lifetime: it survives between calls */
    issued += 1;
    return issued;
}

int fresh_count(void)
{
    int n = 0;                  /* automatic lifetime: born anew on every call */
    n += 1;
    return n;
}

int main(void)
{
    /* calls with side effects get their own statements — chapter 29's rule as it is */
    int t1 = next_ticket();
    int t2 = next_ticket();
    int t3 = next_ticket();
    printf("next_ticket: %d %d %d\n", t1, t2, t3);

    int f1 = fresh_count();
    int f2 = fresh_count();
    int f3 = fresh_count();
    printf("fresh_count: %d %d %d\n", f1, f2, f3);
    return 0;
}
