/* 증인 --- 이 결함이 돌아오면 이 프로그램이 운다.
   각 검사에 *왜 중요한가*를 한 줄로 적어 둔다. 수가 아니라 이유가 자산이다. */
#include "find.h"
#include <stdio.h>

static int fails = 0;

static void check(const char *why, int got, int want)
{
    if (got == want) {
        printf("  ok   %s\n", why);
    } else {
        printf("  FAIL %s  (got %d, wanted %d)\n", why, got, want);
        fails++;
    }
}

int main(void)
{
    /* 픽스처 --- 검사들이 함께 딛고 서는 고정된 판 */
    static const int sorted[] = { 10, 20, 30, 40, 50 };
    const int n = (int)(sizeof sorted / sizeof sorted[0]);

    check("finds a value in the middle",      find(sorted, n, 30),  2);
    check("finds the first",                  find(sorted, n, 10),  0);
    check("* finds the last - one cell of edge", find(sorted, n, 50),  4);
    check("an absent value gives -1",         find(sorted, n, 35), -1);
    check("an empty array gives -1 too",      find(sorted, 0, 10), -1);

    printf("%s\n", fails == 0 ? "all five witnesses are quiet" : "a witness cried");
    return fails == 0 ? 0 : 1;
}
