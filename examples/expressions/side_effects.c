#include <stdio.h>

static int calls;

static bool hit(bool v)          /* 불릴 때마다 calls 를 바꾼다 --- 부수 효과 */
{
    calls++;
    return v;
}

int main(void)
{
    puts("[1] an expression has a value, and may also do something");
    int x = 0;
    int v = (x = 5) + 1;        /* 대입식의 값은 5, 그리고 x 가 바뀐다 */
    printf("v = %d, x = %d\n", v, x);

    int i = 3;
    int post = i++;             /* 값은 3, 효과는 i 를 4 로 */
    int pre  = ++i;             /* 값은 5, 효과는 i 를 5 로 */
    printf("post = %d, pre = %d, i = %d\n", post, pre, i);

    puts("\n[2] && skips its right side --- and the side effects in it");
    calls = 0;
    bool r = hit(false) && hit(true);
    printf("result %d, hit() called %d time(s)\n", r, calls);
    calls = 0;
    r = hit(true) || hit(false);
    printf("result %d, hit() called %d time(s)\n", r, calls);

    puts("\n[3] the comma operator: left, then right; value of the right");
    int c = (calls = 0, calls + 10);
    printf("c = %d\n", c);

    puts("\n[4] one change per object between sequence points");
    int k = 0;
    k++, k++;                   /* 쉼표 연산자 --- 사이에 시퀀스 포인트 */
    k++;                        /* 완전 수식이 끝날 때마다 시퀀스 포인트 */
    k++;
    printf("k = %d\n", k);
    return 0;
}
