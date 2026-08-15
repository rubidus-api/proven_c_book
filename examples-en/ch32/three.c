/* The same job written with each of the three siblings, so the differences show. */
#include <stdio.h>

/* summing 1..5 - while */
static int sum_while(void)
{
    int sum = 0;
    int i = 1;                  /* start: it sits outside the loop */
    while (i <= 5) {            /* condition */
        sum += i;
        i += 1;                 /* update: at the end of the body - easy to forget */
    }
    return sum;
}

/* the same job - for. The housekeeping (start, condition, update) is on one line */
static int sum_for(void)
{
    int sum = 0;
    for (int i = 1; i <= 5; i += 1)
        sum += i;
    return sum;
}

/* the same job - do-while. It runs the body once first */
static int sum_do(void)
{
    int sum = 0;
    int i = 1;
    do {
        sum += i;
        i += 1;
    } while (i <= 5);
    return sum;
}

/* where the three differ, part 1 - when the condition is false from the start */
static int count_while(int n)
{
    int turns = 0, i = 0;
    while (i < n) { turns++; i++; }
    return turns;
}

static int count_do(int n)
{
    int turns = 0, i = 0;
    do { turns++; i++; } while (i < n);
    return turns;
}

/* where the three differ, part 2 - where continue goes.
   A for loop's update always runs, even after continue. When the loop is
   rewritten as a while with the update at the end of the body, continue jumps
   over that update and the loop never stops. */
static int odd_sum_for(void)
{
    int sum = 0;
    for (int i = 1; i <= 9; i++) {
        if (i % 2 == 0)
            continue;           /* i++ still runs */
        sum += i;
    }
    return sum;
}

static int odd_sum_while_fixed(void)
{
    int sum = 0;
    int i = 1;
    while (i <= 9) {
        if (i % 2 == 0) {
            i++;                /* the update has to happen here too */
            continue;
        }
        sum += i;
        i++;
    }
    return sum;
}

int main(void)
{
    printf("[the same job, three ways]\n");
    printf("  while    : 1..5 -> %d\n", sum_while());
    printf("  for      : 1..5 -> %d\n", sum_for());
    printf("  do-while : 1..5 -> %d\n", sum_do());

    printf("\n[what changes when the condition is false from the start]\n");
    printf("  while (i < 0) ran %d time(s)\n", count_while(0));
    printf("  do ... while (i < 0) ran %d time(s)\n", count_do(0));

    printf("\n[continue and the update step]\n");
    printf("  for   : sum of odd numbers 1..9 = %d\n", odd_sum_for());
    printf("  while : sum of odd numbers 1..9 = %d\n", odd_sum_while_fixed());
    printf("  in the for loop i++ runs even after continue;\n");
    printf("  in the while loop the update sits in the body, so continue can skip it -\n");
    printf("  that is how a working loop turns into an endless one when it is rewritten.\n");
    return 0;
}
