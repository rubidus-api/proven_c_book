/* 같은 개수를 읽는데 순서만 다르다 --- 그 차이를 기계에게 물어본다.
   ★ 이 예제는 시간을 잰다. 그래서 나오는 수는 기계마다, 회차마다 다르다.
     절대 시간은 찍지 않고 *배수*만 낸다. 결론(순서가 크게 다르다)만 남기면 된다. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define N     (4u << 20)      /* 원소 400만 개 = 16 MiB --- 캐시보다 크게 */
#define TOUCH 1500000         /* 두 방식 모두 정확히 이만큼 읽는다 */
#define ROUNDS 3

static uint32_t *next_index;  /* 무작위 순회를 위한 고리 */
static uint32_t *data;
static volatile uint64_t sink;

static uint64_t rng = 0x2545F4914F6CDD1DULL;
static uint32_t rnd(void)
{ rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 32); }

static double order_in_order(void)      /* 차례대로 읽는다 */
{
    clock_t t0 = clock();
    uint64_t acc = 0;
    for (uint32_t i = 0; i < TOUCH; i++) acc += data[i % N];
    clock_t t1 = clock();
    sink = acc;
    return (double)(t1 - t0);
}

static double order_scattered(void)     /* 흩어진 자리를 읽는다(같은 횟수) */
{
    clock_t t0 = clock();
    uint64_t acc = 0;
    uint32_t p = 0;
    for (uint32_t i = 0; i < TOUCH; i++) { acc += data[p]; p = next_index[p]; }
    clock_t t1 = clock();
    sink = acc;
    return (double)(t1 - t0);
}

static int cmp(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }

int main(void)
{
    data = malloc((size_t)N * sizeof *data);
    next_index = malloc((size_t)N * sizeof *next_index);
    if (!data || !next_index) { puts("out of memory"); return 1; }

    for (uint32_t i = 0; i < N; i++) { data[i] = i; next_index[i] = i; }
    /* 배열 전체를 도는 고리 하나를 만든다 --- 기계가 다음 자리를 짐작하지 못하게 */
    for (uint32_t i = N - 1; i > 0; i--) {
        uint32_t j = rnd() % i;
        uint32_t t = next_index[i]; next_index[i] = next_index[j]; next_index[j] = t;
    }

    double a[ROUNDS], b[ROUNDS];
    order_in_order(); order_scattered();          /* 데우기 --- 첫 회는 버린다 */
    for (int r = 0; r < ROUNDS; r++) { a[r] = order_in_order(); b[r] = order_scattered(); }
    qsort(a, ROUNDS, sizeof *a, cmp);
    qsort(b, ROUNDS, sizeof *b, cmp);

    double in_order = a[ROUNDS / 2], scattered = b[ROUNDS / 2];   /* 중앙값 */

    printf("reading %u values, %u times each\n\n", N, TOUCH);
    puts("both loops read the same number of values;");
    puts("only the order of the addresses differs.\n");
    if (in_order > 0.0)
        printf("scattered order took %.1f times as long as sequential order\n",
               scattered / in_order);
    else
        puts("the clock is too coarse on this machine to tell them apart");
    puts("\n(no absolute times are printed: they differ by machine and by run.");
    puts(" what stays is the ratio, and the reason for it.)");

    free(next_index); free(data);
    return 0;
}
