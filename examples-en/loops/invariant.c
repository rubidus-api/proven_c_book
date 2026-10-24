/* Seeing an invariant - the proposition that holds on every turn, actually checked.
   assert writes down "this must be true here" as code (chapter 17). */
#include <assert.h>
#include <stdio.h>

/* 1. The sum from 1 to n - the invariant printed as a table.
      On every entry to the body, sum == 1 + 2 + ... + (i-1) holds.
      The right-hand side is counted separately and compared. */
static int sum_to(int n)
{
    int sum = 0;
    int checked = 0;                 /* 1 + ... + (i-1), counted on the side */

    printf("   i | sum | 1+...+(i-1) | invariant\n");
    printf("  ---+-----+-------------+----------\n");
    for (int i = 1; i <= n; i++) {
        assert(sum == checked);      /* <- this is where it holds, every turn */
        printf("  %2d | %3d | %11d | %s\n", i, sum, checked,
               sum == checked ? "holds" : "BROKEN");
        sum += i;                    /* the body grows the equation by one term */
        checked += i;                /* and the update i++ restores the shape */
    }
    /* Termination: i > n. Put i = n+1 into the invariant: sum == 1 + ... + n */
    printf("  loop ended: sum = %d\n", sum);
    return sum;
}

/* 2. What guarantees termination is not the invariant but a shrinking quantity.
      Here that quantity is n - i, and it falls by exactly one per turn. */
static void measure_shrinks(int n)
{
    int prev = n;                    /* how many steps were left last turn */
    for (int i = 0; i < n; i++) {
        int left = n - i;            /* steps left = the shrinking quantity */
        assert(left < prev || i == 0);
        assert(left >= 0);
        prev = left;
    }
    printf("  the quantity (n - i) fell from %d to 0, one step at a time\n", n);
}

/* 3. Binary search - where an invariant really earns its keep.
      Invariant: if the key is in the array at all, it is inside [lo, hi).
      The range narrows every turn (termination); an empty range means absent. */
static int binary_search(const int *a, int n, int key)
{
    int lo = 0, hi = n;              /* the half-open range [lo, hi) */
    int steps = 0;

    while (lo < hi) {
        assert(0 <= lo && lo <= hi && hi <= n);   /* the range stays valid */
        int mid = lo + (hi - lo) / 2;             /* lo+hi could overflow */
        assert(lo <= mid && mid < hi);            /* so mid is inside the range */
        steps++;
        if (a[mid] == key)
            return mid;
        if (a[mid] < key)
            lo = mid + 1;            /* the left half cannot hold the answer */
        else
            hi = mid;                /* the right half cannot hold the answer */
    }
    printf("  (%d not found in %d steps)\n", key, steps);
    return -1;
}

int main(void)
{
    printf("[the invariant of a summing loop]\n");
    int s = sum_to(5);
    printf("  1 + 2 + 3 + 4 + 5 = %d\n", s);

    printf("\n[what makes a loop end is a quantity that shrinks]\n");
    measure_shrinks(4);

    printf("\n[binary search - the invariant is the search range]\n");
    int a[] = { 2, 4, 8, 16, 32, 64, 128 };
    int n = (int)(sizeof a / sizeof a[0]);
    for (int key = 1; key <= 16; key *= 2)
        printf("  key %3d -> index %d\n", key, binary_search(a, n, key));
    printf("  key %3d -> index %d\n", 5, binary_search(a, n, 5));

    printf("\n[the empty range is not a special case - the invariant covers it]\n");
    printf("  searching an empty array for 42 -> index %d\n",
           binary_search(a, 0, 42));
    return 0;
}
