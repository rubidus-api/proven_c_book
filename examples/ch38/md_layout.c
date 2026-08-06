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
    printf("sizeof a       = %2zu  (int 4개짜리 행 3개)\n", sizeof a);
    printf("sizeof a[0]    = %2zu  (행 하나 = int 4개)\n",  sizeof a[0]);
    printf("sizeof a[0][0] = %2zu  (원소 하나)\n\n",        sizeof a[0][0]);

    /* ── ② 기억은 한 덩어리다 — row-major ────────────────── */
    printf("주소를 바이트 오프셋으로 보면:\n");
    const char *base = (const char *)&a[0][0];
    for (int i = 0; i < 3; i++) {
        printf("  행 %d:", i);
        for (int j = 0; j < 4; j++)
            printf(" a[%d][%d]=+%02td", i, j, (const char *)&a[i][j] - base);
        printf("\n");
    }
    printf("  마지막 첨자가 가장 빨리 변한다(row-major).\n\n");

    /* ── ③ 첨자식이 풀리는 과정 — a[2][1] 을 손으로 ───────── */
    puts("a[2][1] 을 단계별로 푼다(주소는 a 로부터의 바이트 오프셋):");
    printf("  a            타입 int(*)[4],  +%td\n",
           (const char *)a - base);
    printf("  a + 2        한 걸음 = sizeof(int[4]) = %zu → +%zu, 오프셋 +%td\n",
           sizeof(int[4]), 2 * sizeof(int[4]), (const char *)(a + 2) - base);
    printf("  *(a + 2)     타입 int[4] → 무너져 int*, 오프셋 +%td\n",
           (const char *)*(a + 2) - base);
    printf("  *(a+2) + 1   한 걸음 = sizeof(int) = %zu → +%zu, 오프셋 +%td\n",
           sizeof(int), 1 * sizeof(int), (const char *)(*(a + 2) + 1) - base);
    printf("  *(*(a+2)+1)  값 = %d,  a[2][1] = %d  (같다)\n\n",
           *(*(a + 2) + 1), a[2][1]);

    /* ── ④ 같은 주소, 다른 타입 ──────────────────────────── */
    printf("a, a[0], &a[0][0], &a 는 모두 같은 주소다(오프셋 0):\n");
    printf("  a        +%td   (int(*)[4])\n",    (const char *)a - base);
    printf("  a[0]     +%td   (int*)\n",         (const char *)a[0] - base);
    printf("  &a[0][0] +%td   (int*)\n",         (const char *)&a[0][0] - base);
    printf("  &a       +%td   (int(*)[3][4])\n", (const char *)&a - base);
    printf("그러나 한 걸음의 크기가 다르다:\n");
    printf("  a + 1        → +%td바이트 (행 하나)\n",
           (const char *)(a + 1) - (const char *)a);
    printf("  a[0] + 1     → +%td바이트 (원소 하나)\n",
           (const char *)(a[0] + 1) - (const char *)a[0]);
    printf("  &a + 1       → +%td바이트 (배열 전체)\n",
           (const char *)(&a + 1) - (const char *)&a);
    return 0;
}
