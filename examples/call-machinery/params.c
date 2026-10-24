// 매개변수에 적은 것과 실제로 받는 것은 다를 수 있다.
#include <stdio.h>

// [10] 이라고 적었지만 받는 것은 포인터 하나다.
// (여기서 sizeof a 를 쓰면 gcc 가 -Wsizeof-array-argument 로 나무란다.)
static void looks_like_array(int a[10])
{
    int *p = a;                    // 같은 것이다 --- 대입에 경고가 없다
    printf("  inside: sizeof(the parameter) = %zu\n", sizeof p);
}

// 이렇게 적어야 "적어도 10개는 있다"가 계약이 된다 (C99 부터).
static int sum_ten(int a[static 10])
{
    int total = 0;
    for (int i = 0; i < 10; i++)
        total += a[i];
    return total;
}

// 길이를 따로 받는 것이 정석이다.
static int sum_n(const int *a, size_t n)
{
    int total = 0;
    for (size_t i = 0; i < n; i++)
        total += a[i];
    return total;
}

int main(void)
{
    int v[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    printf("outside: sizeof(v) = %zu\n", sizeof v);
    looks_like_array(v);
    printf("  the array itself never crossed the call --- only its address did\n");

    printf("sum via [static 10]: %d\n", sum_ten(v));
    printf("sum via pointer+length: %d\n", sum_n(v, sizeof v / sizeof v[0]));
    return 0;
}
