// 우선순위·결합성·짧은 회로를 눈으로 확인한다.
// 표를 외우는 대신, 괄호를 넣은 것과 안 넣은 것을 나란히 찍어 본다.
#include <stdio.h>

static int calls = 0;

static int loud(int value)          // 불릴 때마다 흔적을 남긴다
{
    calls += 1;
    return value;
}

int main(void)
{
    int x = 6;

    puts("1. precedence --- what binds tighter");
    printf("  2 + 3 * 4      = %d   (same as 2 + (3 * 4))\n", 2 + 3 * 4);
    printf("  (2 + 3) * 4    = %d\n", (2 + 3) * 4);
    printf("  1 << 2 + 3     = %d   (same as 1 << (2 + 3): + binds tighter than <<)\n",
           1 << (2 + 3));

    puts("2. the classic trap --- == binds tighter than &");
    printf("  x & 1 == 0     is read as x & (1 == 0) = %d\n", x & (1 == 0));
    printf("  what was meant: (x & 1) == 0         = %d\n", (x & 1) == 0);

    puts("3. associativity --- which side groups first");
    printf("  10 - 4 - 3     = %d   (left to right: (10 - 4) - 3)\n", 10 - 4 - 3);
    int a, b;
    a = b = 5;                      // 대입은 오른쪽부터 묶인다
    printf("  a = b = 5      gives a=%d b=%d   (right to left)\n", a, b);

    puts("4. comparison does not chain the way mathematics does");
    printf("  1 < 2 < 3      = %d   ((1 < 2) is 1, and 1 < 3 is true)\n", (1 < 2) < 3);
    printf("  3 > 2 > 1      = %d   ((3 > 2) is 1, and 1 > 1 is false)\n", (3 > 2) > 1);

    puts("5. short circuit --- the right side may never run");
    calls = 0;
    if (0 && loud(1))
        puts("  not reached");
    printf("  after 0 && loud(1): loud() was called %d time(s)\n", calls);
    calls = 0;
    if (1 || loud(1))
        (void)0;
    printf("  after 1 || loud(1): loud() was called %d time(s)\n", calls);
    return 0;
}
