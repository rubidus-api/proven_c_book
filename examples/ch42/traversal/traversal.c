/* 같은 합, 다른 순서 --- 순회 순서가 만드는 차이를 기계에게 물어본다.
   ★ 이 예제는 시간을 잰다. 나오는 수는 기계마다, 회차마다, 그리고 *최적화
     수준마다* 다르다. 12장의 order.c 와 같은 규율을 지킨다 --- 절대 시간은
     찍지 않고 배수만 낸다. 절대 시간은 어느 기계의 것인지 밝히지 않으면
     뜻이 없다.
   ★ 두 루프가 정확히 같은 일을 하게 해 두었다. 값이 모두 같으면 컴파일러가
     합을 상수로 접어 루프를 통째로 지운다 --- 처음에 그렇게 재어 「행 우선이
     0.0 ms」라는 있을 수 없는 수를 얻었다. 그래서 값을 흩고 결과를 소비한다. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N      4000          /* 4000×4000 int = 64 MB --- 캐시보다 훨씬 크게 */
#define ROUNDS 5

static int (*a)[N];
static volatile long long sink;

static double elapsed_ms(struct timespec s, struct timespec e)
{
    return (double)(e.tv_sec - s.tv_sec) * 1e3
         + (double)(e.tv_nsec - s.tv_nsec) / 1e6;
}

int main(void)
{
    a = malloc(sizeof *a * N);
    if (!a) { puts("out of memory"); return 1; }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            a[i][j] = (i * 31 + j) & 0xff;   /* 값을 흩는다 */

    struct timespec s, e;
    double best_row = 1e18, best_col = 1e18;

    for (int r = 0; r < ROUNDS; r++) {
        long long sum = 0;
        timespec_get(&s, TIME_UTC);
        for (int i = 0; i < N; i++)          /* 행 우선 --- 줄을 따라 간다 */
            for (int j = 0; j < N; j++)
                sum += a[i][j];
        timespec_get(&e, TIME_UTC);
        sink = sum;
        if (elapsed_ms(s, e) < best_row) best_row = elapsed_ms(s, e);

        sum = 0;
        timespec_get(&s, TIME_UTC);
        for (int j = 0; j < N; j++)          /* 열 우선 --- 줄을 가로지른다 */
            for (int i = 0; i < N; i++)
                sum += a[i][j];
        timespec_get(&e, TIME_UTC);
        sink = sum;
        if (elapsed_ms(s, e) < best_col) best_col = elapsed_ms(s, e);
    }

    printf("  summing a %d x %d int array, %d rounds, best of each\n",
           N, N, ROUNDS);
    printf("  both loops add the same %lld values; only the order differs.\n",
           (long long)N * N);
    printf("  column-major took %.1f times as long as row-major\n",
           best_col / best_row);
    free(a);
    return 0;
}
