// 비트맵 --- 비트 연산이 실무에서 가장 자주 쓰이는 모습.
// 참·거짓 백만 개를 담는 데 바이트 백만 개를 쓸 이유가 없다.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbit.h>

#define BITS_PER_WORD 64
#define WORDS(n)      (((n) + BITS_PER_WORD - 1) / BITS_PER_WORD)

#define N 1000
static uint64_t set[WORDS(N)];

// 번호 i 는 몇 번째 워드의 몇 번째 비트인가.
// 워드 크기가 2 의 거듭제곱이므로 나눗셈과 나머지를 시프트와 마스크로 적을 수 있다.
static size_t   word_of(size_t i) { return i >> 6; }        // i / 64
static unsigned bit_of (size_t i) { return i & 63u; }       // i % 64

static void bit_set   (size_t i) { set[word_of(i)] |=  UINT64_C(1) << bit_of(i); }
static void bit_clear (size_t i) { set[word_of(i)] &= ~(UINT64_C(1) << bit_of(i)); }
static int  bit_test  (size_t i) { return (set[word_of(i)] >> bit_of(i)) & 1u; }

static size_t bit_count(void)
{
    size_t total = 0;
    for (size_t w = 0; w < WORDS(N); w++)
        total += stdc_count_ones(set[w]);          // 워드 하나를 한 번에
    return total;
}

int main(void)
{
    printf("%d flags need %zu bytes as a bitmap, %d bytes as one char each\n",
           N, sizeof set, N);

    // 에라토스테네스의 체 --- 비트맵의 교과서적 쓰임
    memset(set, 0, sizeof set);
    for (size_t i = 2; i < N; i++)
        bit_set(i);                                // 일단 전부 「소수일 수 있다」
    for (size_t p = 2; p * p < N; p++)
        if (bit_test(p))
            for (size_t m = p * p; m < N; m += p)
                bit_clear(m);

    // 워드 끝의 남는 비트는 세지 않도록 지운다 --- 잊기 쉬운 자리다.
    for (size_t i = N; i < WORDS(N) * BITS_PER_WORD; i++)
        bit_clear(i);

    printf("primes below %d: %zu\n", N, bit_count());
    printf("  is 997 prime? %s\n", bit_test(997) ? "yes" : "no");
    printf("  is 999 prime? %s\n", bit_test(999) ? "yes" : "no");

    printf("the first ten:");
    size_t shown = 0;
    for (size_t i = 0; i < N && shown < 10; i++)
        if (bit_test(i)) { printf(" %zu", i); shown++; }
    puts("");
    return 0;
}
