/* What a multidimensional array really is — sizes, addresses, and how a
   subscript expression unfolds. */
#include <stdio.h>

int main(void)
{
    int a[3][4] = {
        { 11, 12, 13, 14 },
        { 21, 22, 23, 24 },
        { 31, 32, 33, 34 },
    };

    /* -- (1) what, and how many ------------------------------------ */
    printf("sizeof a       = %2zu  (three rows of four ints)\n", sizeof a);
    printf("sizeof a[0]    = %2zu  (one row = four ints)\n",  sizeof a[0]);
    printf("sizeof a[0][0] = %2zu  (one element)\n\n",        sizeof a[0][0]);

    /* -- (2) memory is one run — row-major -------------------------- */
    printf("as byte offsets from the start:\n");
    const char *base = (const char *)&a[0][0];
    for (int i = 0; i < 3; i++) {
        printf("  row %d:", i);
        for (int j = 0; j < 4; j++)
            printf(" a[%d][%d]=+%02td", i, j, (const char *)&a[i][j] - base);
        printf("\n");
    }
    printf("  the last subscript varies fastest (row-major).\n\n");

    /* -- (3) unfolding a[2][1] by hand ------------------------------ */
    puts("a[2][1], step by step (addresses as byte offsets from a):");
    printf("  a            type int(*)[4],  +%td\n",
           (const char *)a - base);
    printf("  a + 2        step = sizeof(int[4]) = %zu -> +%zu, offset +%td\n",
           sizeof(int[4]), 2 * sizeof(int[4]), (const char *)(a + 2) - base);
    printf("  *(a + 2)     type int[4] -> decays to int*, offset +%td\n",
           (const char *)*(a + 2) - base);
    printf("  *(a+2) + 1   step = sizeof(int) = %zu -> +%zu, offset +%td\n",
           sizeof(int), 1 * sizeof(int), (const char *)(*(a + 2) + 1) - base);
    printf("  *(*(a+2)+1)  value = %d,  a[2][1] = %d  (the same)\n\n",
           *(*(a + 2) + 1), a[2][1]);

    /* -- (4) same address, different types -------------------------- */
    printf("a, a[0], &a[0][0], &a are all the same address (offset 0):\n");
    printf("  a        +%td   (int(*)[4])\n",    (const char *)a - base);
    printf("  a[0]     +%td   (int*)\n",         (const char *)a[0] - base);
    printf("  &a[0][0] +%td   (int*)\n",         (const char *)&a[0][0] - base);
    printf("  &a       +%td   (int(*)[3][4])\n", (const char *)&a - base);
    printf("but one step means a different distance:\n");
    printf("  a + 1        -> +%td bytes (one row)\n",
           (const char *)(a + 1) - (const char *)a);
    printf("  a[0] + 1     -> +%td bytes (one element)\n",
           (const char *)(a[0] + 1) - (const char *)a[0]);
    printf("  &a + 1       -> +%td bytes (the whole array)\n",
           (const char *)(&a + 1) - (const char *)&a);
    return 0;
}
