#include <stdio.h>

int next_ticket(void)
{
    static int issued = 0;      /* 정적 수명: 호출 사이에도 살아남는다 */
    issued += 1;
    return issued;
}

int fresh_count(void)
{
    int n = 0;                  /* 자동 수명: 호출마다 새로 태어난다 */
    n += 1;
    return n;
}

int main(void)
{
    /* 부수효과 있는 호출은 문장을 나눈다 — 28장의 수칙 그대로 */
    int t1 = next_ticket();
    int t2 = next_ticket();
    int t3 = next_ticket();
    printf("next_ticket: %d %d %d\n", t1, t2, t3);

    int f1 = fresh_count();
    int f2 = fresh_count();
    int f3 = fresh_count();
    printf("fresh_count: %d %d %d\n", f1, f2, f3);
    return 0;
}
