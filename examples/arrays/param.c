#include <stdio.h>

/* 세 선언은 컴파일러에게 완전히 같은 것이다: 전부 int * 다.
   (매개변수 이름에 직접 sizeof 를 쓰면 gcc 가 경고하므로,
    같은 타입의 지역 변수로 한 번 받아 크기를 잰다) */
static void by_ptr(int *a)     { int *p = a; printf("  int *a      : sizeof = %zu\n", sizeof p); }
static void by_arr(int a[])    { int *p = a; printf("  int a[]     : sizeof = %zu\n", sizeof p); }
static void by_size(int a[10]) { int *p = a; printf("  int a[10]   : sizeof = %zu\n", sizeof p); }

/* 매개변수는 그냥 포인터 변수라서 다른 주소를 대입할 수도 있다 */
static int second_of(int a[10])
{
    a = a + 1;          /* 배열이라면 불가능하다 — 배열 이름에는 대입할 수 없다 */
    return a[0];
}

/* 2차원: 벗겨지는 것은 가장 바깥(왼쪽 첫) 차원뿐이다.
   int m[3][4]  ->  int (*m)[4] */
static void by_2d(int m[3][4])
{
    int (*p)[4] = m;    /* 매개변수의 진짜 타입이 이것이다 */
    printf("  int m[3][4] : sizeof = %zu (a pointer), sizeof p[0] = %zu (one row)\n",
           sizeof p, sizeof p[0]);
    printf("                m[1][2] = %d\n", m[1][2]);
}

int main(void)
{
    int arr[10] = {0,1,2,3,4,5,6,7,8,9};
    int grid[3][4] = { {0,1,2,3}, {4,5,6,7}, {8,9,10,11} };

    printf("at the caller (a real array):\n");
    printf("  int arr[10]    : sizeof = %zu\n", sizeof arr);
    printf("  int grid[3][4] : sizeof = %zu, sizeof grid[0] = %zu\n",
           sizeof grid, sizeof grid[0]);

    printf("inside the function (all pointers):\n");
    by_ptr(arr);
    by_arr(arr);
    by_size(arr);
    by_2d(grid);

    printf("assigning to the parameter: second_of(arr) = %d\n", second_of(arr));
    printf("the original is untouched : arr[0] = %d\n", arr[0]);
    return 0;
}
