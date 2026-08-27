// A bitmap --- the shape bit work most often takes in real programs.
// There is no reason to spend a million bytes on a million true/false answers.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbit.h>

#define BITS_PER_WORD 64
#define WORDS(n)      (((n) + BITS_PER_WORD - 1) / BITS_PER_WORD)

#define N 1000
static uint64_t set[WORDS(N)];

// Which word, and which bit inside it, does index i live in?
// The word size is a power of two, so the division and remainder become a shift and a mask.
static size_t   word_of(size_t i) { return i >> 6; }        // i / 64
static unsigned bit_of (size_t i) { return i & 63u; }       // i % 64

static void bit_set   (size_t i) { set[word_of(i)] |=  UINT64_C(1) << bit_of(i); }
static void bit_clear (size_t i) { set[word_of(i)] &= ~(UINT64_C(1) << bit_of(i)); }
static int  bit_test  (size_t i) { return (set[word_of(i)] >> bit_of(i)) & 1u; }

static size_t bit_count(void)
{
    size_t total = 0;
    for (size_t w = 0; w < WORDS(N); w++)
        total += stdc_count_ones(set[w]);          // one whole word at a time
    return total;
}

int main(void)
{
    printf("%d flags need %zu bytes as a bitmap, %d bytes as one char each\n",
           N, sizeof set, N);

    // The sieve of Eratosthenes --- the textbook use of a bitmap
    memset(set, 0, sizeof set);
    for (size_t i = 2; i < N; i++)
        bit_set(i);                                // everything is "maybe prime" to start
    for (size_t p = 2; p * p < N; p++)
        if (bit_test(p))
            for (size_t m = p * p; m < N; m += p)
                bit_clear(m);

    // Clear the spare bits at the end of the last word so they are not counted ---
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
