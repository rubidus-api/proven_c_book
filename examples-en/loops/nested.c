/* Nested loops - double and triple, and the mistakes that nesting creates. */
#include <stdio.h>

/* 1. A double for - the inner loop restarts on every outer turn */
static void times_table(void)
{
    for (int row = 2; row <= 4; row++) {
        printf("  ");
        for (int col = 1; col <= 5; col++)
            printf("%s%2d x %d = %2d", col == 1 ? "" : "   ", row, col, row * col);
        printf("\n");                    /* where the inner loop ends = the end of a row */
    }
}

/* 2. Declare the inner counter outside and the restart disappears */
static void forgot_to_reset(void)
{
    int visits = 0;
    int col = 1;                         /* the initialization is outside */
    for (int row = 2; row <= 4; row++)
        for (; col <= 5; col++)          /* from the second turn on, col is already 6 */
            visits++;
    printf("  counter declared outside : %d cells visited\n", visits);

    visits = 0;
    for (int row = 2; row <= 4; row++)
        for (int c = 1; c <= 5; c++)     /* declared inside, it starts over every turn */
            visits++;
    printf("  counter declared inside  : %d cells visited\n", visits);
}

/* 3. Raising the outer counter inside the inner loop (the shape of a real case).
      A limit is added here so that it stops - real code has no such guard. */
static void wrong_counter(void)
{
    int steps = 0;
    for (int j = 0; j < 3; j++) {
        for (int k = 0; k < 3; j++) {    /* it raises j, not k */
            steps++;
            if (steps >= 100)            /* without this line it never ends */
                break;
        }
        if (steps >= 100)
            break;
    }
    printf("  wrong counter (k stands still): %d steps and still not done\n", steps);

    steps = 0;
    for (int j = 0; j < 3; j++)
        for (int k = 0; k < 3; k++)
            steps++;
    printf("  right counter                : %d steps, 3 x 3 as intended\n", steps);
}

/* 4. A triple for - the classic way to sweep three values. Pythagorean triples. */
static void triples(int n)
{
    long long tries = 0;
    int found = 0;

    for (int a = 1; a <= n; a++)
        for (int b = a; b <= n; b++)             /* starting b at a removes duplicates */
            for (int c = b; c <= n; c++) {
                tries++;
                if (a * a + b * b == c * c) {
                    printf("  %2d^2 + %2d^2 = %2d^2\n", a, b, c);
                    found++;
                }
            }
    printf("  n = %d: %d triple(s) found in %lld checks\n", n, found, tries);
}

int main(void)
{
    printf("[a double loop - the inner one restarts every outer turn]\n");
    times_table();

    printf("\n[where the inner counter is declared decides whether it restarts]\n");
    forgot_to_reset();

    printf("\n[raising the wrong counter in the inner loop]\n");
    wrong_counter();

    printf("\n[a triple loop - Pythagorean triples up to n]\n");
    triples(20);

    printf("\n[how fast the work grows]\n");
    for (int n = 10; n <= 40; n *= 2) {
        long long steps = 0;
        for (int a = 1; a <= n; a++)
            for (int b = 1; b <= n; b++)
                for (int c = 1; c <= n; c++)
                    steps++;
        printf("  n = %2d -> %lld steps (n^3)\n", n, steps);
    }
    return 0;
}
