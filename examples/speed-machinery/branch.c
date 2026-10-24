/* 같은 자료, 같은 조건문 --- 순서만 다르다. 그 차이를 기계에게 물어본다.
   ★ 시간을 재는 예제다. 절대 시간은 찍지 않고 배수만 낸다(회차·기계마다 다르다). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define N      (1u << 20)     /* 원소 백만 개 */
#define ROUNDS 5

static volatile uint64_t sink;

static uint64_t rng = 0x9E3779B97F4A7C15ULL;
static uint32_t rnd(void)
{ rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 32); }

static int cmp_u8(const void *a, const void *b)
{ int x = *(const unsigned char *)a, y = *(const unsigned char *)b; return (x > y) - (x < y); }
static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }

/* 조건에 걸리는 것만 더한다 --- 조건이 맞히기 쉬운지 어려운지가 이 예제의 주제다 */
static double sum_over(const unsigned char *v)
{
    clock_t t0 = clock();
    uint64_t acc = 0;
    for (unsigned round = 0; round < 8; round++)
        for (uint32_t i = 0; i < N; i++)
            if (v[i] >= 128) acc += v[i];
    clock_t t1 = clock();
    sink = acc;
    return (double)(t1 - t0);
}

int main(void)
{
    unsigned char *shuffled = malloc(N), *sorted = malloc(N);
    if (!shuffled || !sorted) { puts("out of memory"); return 1; }
    for (uint32_t i = 0; i < N; i++) shuffled[i] = (unsigned char)(rnd() & 0xFF);
    for (uint32_t i = 0; i < N; i++) sorted[i] = shuffled[i];
    qsort(sorted, N, 1, cmp_u8);

    double a[ROUNDS], b[ROUNDS];
    sum_over(shuffled); sum_over(sorted);                    /* 데우기 */
    for (int r = 0; r < ROUNDS; r++) { a[r] = sum_over(shuffled); b[r] = sum_over(sorted); }
    qsort(a, ROUNDS, sizeof *a, cmp_d);
    qsort(b, ROUNDS, sizeof *b, cmp_d);
    double mixed = a[ROUNDS / 2], ordered = b[ROUNDS / 2];

    puts("the two arrays hold exactly the same values;");
    puts("one is shuffled, the other is sorted.");
    puts("the loop is the same: add the values that are 128 or more.\n");
    if (ordered > 0.0)
        printf("the shuffled array took %.2f times as long as the sorted one\n",
               mixed / ordered);
    else
        puts("the clock is too coarse on this machine to tell them apart");
    puts("\nsame data, same comparisons, same additions --- only the order differs.");
    puts("(no absolute times: they differ by machine and by run.)");

    free(sorted); free(shuffled);
    return 0;
}
