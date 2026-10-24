#include <stdio.h>

/* [static N]: "여기에 최소 N개짜리 배열이 온다"는 약속.
   함수 매개변수의 배열 선언자에서만 쓸 수 있다. */
static int sum3(const int a[static 3])
{
    return a[0] + a[1] + a[2];
}

/* 가변 길이 배열(VLA)을 매개변수로: 크기를 앞에서 받아 뒤에서 쓴다 */
static int sum_n(size_t n, const int a[n])
{
    int s = 0;
    for (size_t i = 0; i < n; i++) s += a[i];
    return s;
}

/* 2차원 VLA 매개변수 — 손으로 인덱스를 계산하지 않아도 된다 */
static int trace(size_t n, const int m[n][n])
{
    int s = 0;
    for (size_t i = 0; i < n; i++) s += m[i][i];
    return s;
}

int main(void)
{
    int fixed[3] = {1, 2, 3};
    printf("sum3        = %d\n", sum3(fixed));

    size_t n = 4;
    int local[n];                    /* 지역 VLA: 크기가 실행 중에 정해진다 */
    for (size_t i = 0; i < n; i++) local[i] = (int)(i * i);

    printf("sizeof local = %zu bytes (= %zu ints)\n", sizeof local, sizeof local / sizeof local[0]);
    printf("sum_n       = %d\n", sum_n(n, local));

    int grid[3][3] = { {1,2,3}, {4,5,6}, {7,8,9} };
    printf("trace       = %d\n", trace(3, grid));
    return 0;
}
