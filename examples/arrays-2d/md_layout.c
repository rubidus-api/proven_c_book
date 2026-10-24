/* 다차원 배열의 실체 — 크기, 주소, 그리고 첨자가 풀리는 과정. */
#include <stdio.h>

int main(void)
{
    int a[3][4] = {
        { 11, 12, 13, 14 },
        { 21, 22, 23, 24 },
        { 31, 32, 33, 34 },
    };

    /* ── ① 무엇이 몇 개인가 ──────────────────────────────── */
    printf("sizeof a       = %2zu  (3 rows of 4 ints)\n", sizeof a);
    printf("sizeof a[0]    = %2zu  (one row = 4 ints)\n",  sizeof a[0]);
    printf("sizeof a[0][0] = %2zu  (one element)\n\n",        sizeof a[0][0]);

    /* ── ② 기억은 한 덩어리다 — row-major ────────────────── */
    printf("addresses as byte offsets:\n");
    const char *base = (const char *)&a[0][0];
    for (int i = 0; i < 3; i++) {
        printf("  row %d:", i);
        for (int j = 0; j < 4; j++)
            printf(" a[%d][%d]=+%02td", i, j, (const char *)&a[i][j] - base);
        printf("\n");
    }
    printf("  the last subscript varies fastest (row-major).\n\n");

    /* ── ③ 첨자식이 풀리는 과정 — a[2][1] 을 손으로 ───────── */
    puts("resolving a[2][1] step by step (addresses are byte offsets from a):");
    printf("  a            type int(*)[4],  +%td\n",
           (const char *)a - base);
    printf("  a + 2        one step = sizeof(int[4]) = %zu -> +%zu, offset +%td\n",
           sizeof(int[4]), 2 * sizeof(int[4]), (const char *)(a + 2) - base);
    printf("  *(a + 2)     type int[4] -> decays to int*, offset +%td\n",
           (const char *)*(a + 2) - base);
    printf("  *(a+2) + 1   one step = sizeof(int) = %zu -> +%zu, offset +%td\n",
           sizeof(int), 1 * sizeof(int), (const char *)(*(a + 2) + 1) - base);
    printf("  *(*(a+2)+1)  value = %d,  a[2][1] = %d  (the same)\n\n",
           *(*(a + 2) + 1), a[2][1]);

    /* ── ④ 같은 주소, 다른 타입 ──────────────────────────── */
    printf("a, a[0], &a[0][0] and &a are all the same address (offset 0):\n");
    printf("  a        +%td   (int(*)[4])\n",    (const char *)a - base);
    printf("  a[0]     +%td   (int*)\n",         (const char *)a[0] - base);
    printf("  &a[0][0] +%td   (int*)\n",         (const char *)&a[0][0] - base);
    printf("  &a       +%td   (int(*)[3][4])\n", (const char *)&a - base);
    printf("but one step means a different size for each:\n");
    printf("  a + 1        -> +%td bytes (one row)\n",
           (const char *)(a + 1) - (const char *)a);
    printf("  a[0] + 1     -> +%td bytes (one element)\n",
           (const char *)(a[0] + 1) - (const char *)a[0]);
    printf("  &a + 1       -> +%td bytes (the whole array)\n",
           (const char *)(&a + 1) - (const char *)&a);
    return 0;
}
