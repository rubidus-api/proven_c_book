/* 불변식을 눈으로 보기 — 매 바퀴 성립하는 명제를 실제로 검사한다.
   assert 는 「이 자리에서 이 명제가 참이어야 한다」를 코드로 적는 도구다(17장). */
#include <assert.h>
#include <stdio.h>

/* ① 1부터 n 까지의 합 — 불변식을 표로 찍어 본다.
      몸통에 들어설 때마다 sum == 1 + 2 + ... + (i-1) 이 성립한다.
      그 오른쪽을 따로 세어 두었다가 맞대어 본다. */
static int sum_to(int n)
{
    int sum = 0;
    int checked = 0;                 /* 1 + ... + (i-1) 을 따로 셈한 값 */

    printf("   i | sum | 1+...+(i-1) | invariant\n");
    printf("  ---+-----+-------------+----------\n");
    for (int i = 1; i <= n; i++) {
        assert(sum == checked);      /* ← 매 바퀴 여기서 성립한다 */
        printf("  %2d | %3d | %11d | %s\n", i, sum, checked,
               sum == checked ? "holds" : "BROKEN");
        sum += i;                    /* 몸통이 등식을 한 칸 자라게 하고 */
        checked += i;                /* 갱신 i++ 가 다시 같은 꼴로 만든다 */
    }
    /* 종료 조건: i > n. 불변식에 i = n+1 을 넣으면 sum == 1 + ... + n */
    printf("  loop ended: sum = %d\n", sum);
    return sum;
}

/* ② 종료를 보장하는 것은 불변식이 아니라 「줄어드는 양」이다.
      아래 루프에서 그 양은 n - i 이고, 매 바퀴 정확히 1씩 준다. */
static void measure_shrinks(int n)
{
    int prev = n;                    /* 직전 바퀴의 남은 걸음 수 */
    for (int i = 0; i < n; i++) {
        int left = n - i;            /* 남은 걸음 = 줄어드는 양 */
        assert(left < prev || i == 0);
        assert(left >= 0);
        prev = left;
    }
    printf("  the quantity (n - i) fell from %d to 0, one step at a time\n", n);
}

/* ③ 이진 탐색 — 불변식이 실제로 값을 하는 자리.
      불변식: 찾는 값이 배열 안에 있다면, 그것은 반드시 [lo, hi) 안에 있다.
      구간이 매 바퀴 좁아지고(종료), 빈 구간이 되면 없는 것이다. */
static int binary_search(const int *a, int n, int key)
{
    int lo = 0, hi = n;              /* 반열림 구간 [lo, hi) */
    int steps = 0;

    while (lo < hi) {
        assert(0 <= lo && lo <= hi && hi <= n);   /* 구간이 늘 유효하다 */
        int mid = lo + (hi - lo) / 2;             /* lo+hi 로 적으면 넘칠 수 있다 */
        assert(lo <= mid && mid < hi);            /* 그래서 mid 는 구간 안이다 */
        steps++;
        if (a[mid] == key)
            return mid;
        if (a[mid] < key)
            lo = mid + 1;            /* 왼쪽 절반은 답이 아니다 */
        else
            hi = mid;                /* 오른쪽 절반은 답이 아니다 */
    }
    printf("  (%d not found in %d steps)\n", key, steps);
    return -1;
}

int main(void)
{
    printf("[the invariant of a summing loop]\n");
    int s = sum_to(5);
    printf("  1 + 2 + 3 + 4 + 5 = %d\n", s);

    printf("\n[what makes a loop end is a quantity that shrinks]\n");
    measure_shrinks(4);

    printf("\n[binary search - the invariant is the search range]\n");
    int a[] = { 2, 4, 8, 16, 32, 64, 128 };
    int n = (int)(sizeof a / sizeof a[0]);
    for (int key = 1; key <= 16; key *= 2)
        printf("  key %3d -> index %d\n", key, binary_search(a, n, key));
    printf("  key %3d -> index %d\n", 5, binary_search(a, n, 5));

    printf("\n[the empty range is not a special case - the invariant covers it]\n");
    printf("  searching an empty array for 42 -> index %d\n",
           binary_search(a, 0, 42));
    return 0;
}
