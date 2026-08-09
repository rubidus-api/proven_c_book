/* 매개변수에서 배열이 포인터로 고쳐지는 규칙은 「한 겹만」 적용된다.
   그리고 C99 이후에는 크기를 함께 넘겨 진짜 2차원 매개변수를 쓸 수 있다. */
#include <stdio.h>

/* ① 배열의 배열 → 「배열에 대한 포인터」  (포인터의 포인터가 아니다) */
static void take_2d(int (*m)[3], size_t rows)
{
    printf("  int m[][3]  → int (*m)[3] : sizeof m = %zu, sizeof *m = %zu\n",
           sizeof m, sizeof *m);
    printf("  m[1][2] = %d   (행 하나가 %zu 바이트)\n", m[1][2], sizeof *m);
    (void)rows;
}

/* ② 포인터의 배열 → 「포인터에 대한 포인터」  (argv 가 이 모양이다) */
static void take_argvish(char **v)
{
    printf("  char *v[]   → char **v   : v[0]=\"%s\", v[1]=\"%s\"\n", v[0], v[1]);
}

/* ③ C99 이후 — 크기를 먼저 받으면 진짜 가변 2차원 매개변수가 된다 */
static long sum_2d(size_t rows, size_t cols, int m[rows][cols])
{
    long s = 0;
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
            s += m[i][j];
    return s;
}

int main(void)
{
    int  grid[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
    int  wide[2][4] = { { 1, 1, 1, 1 }, { 2, 2, 2, 2 } };
    char *names[]   = { "hana", "dul" };

    puts("① 배열의 배열을 넘긴다");
    take_2d(grid, 2);

    puts("\n② 포인터의 배열을 넘긴다");
    take_argvish(names);

    puts("\n③ 크기를 함께 넘기면 폭이 달라도 같은 함수가 받는다");
    printf("  sum_2d(2,3,grid) = %ld\n", sum_2d(2, 3, grid));
    printf("  sum_2d(2,4,wide) = %ld   <- take_2d 로는 못 받는 모양이다\n",
           sum_2d(2, 4, wide));

    return 0;
}
