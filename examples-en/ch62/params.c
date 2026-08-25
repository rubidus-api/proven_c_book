// What is written as a parameter and what is received can differ.
#include <stdio.h>

// It says [10], but what arrives is a single pointer.
// (Writing sizeof a here earns -Wsizeof-array-argument from gcc.)
static void looks_like_array(int a[10])
{
    int *p = a;                    // the same thing --- the assignment draws no warning
    printf("  inside: sizeof(the parameter) = %zu\n", sizeof p);
}

// Written this way, "there are at least ten" becomes a contract (since C99).
static int sum_ten(int a[static 10])
{
    int total = 0;
    for (int i = 0; i < 10; i++)
        total += a[i];
    return total;
}

// Taking the length separately is the sound way.
static int sum_n(const int *a, size_t n)
{
    int total = 0;
    for (size_t i = 0; i < n; i++)
        total += a[i];
    return total;
}

int main(void)
{
    int v[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    printf("outside: sizeof(v) = %zu\n", sizeof v);
    looks_like_array(v);
    printf("  the array itself never crossed the call --- only its address did\n");

    printf("sum via [static 10]: %d\n", sum_ten(v));
    printf("sum via pointer+length: %d\n", sum_n(v, sizeof v / sizeof v[0]));
    return 0;
}
