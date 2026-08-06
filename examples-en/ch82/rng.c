#include <proven.h>
#include <stdio.h>

int main(void)
{
    /* Reproducible random numbers: the same seed = the same sequence. For tests and simulations */
    proven_xoshiro256ss_t g;
    proven_xoshiro256ss_seed(&g, 12345);
    proven_rng_t rng = proven_xoshiro256ss_rng(&g);

    printf("seeded run 1:");
    for (int i = 0; i < 5; i++) printf(" %llu", (unsigned long long)proven_rng_below(rng, 100));
    printf("\n");

    proven_xoshiro256ss_seed(&g, 12345);          /* rewound with the same seed */
    printf("seeded run 2:");
    for (int i = 0; i < 5; i++) printf(" %llu", (unsigned long long)proven_rng_below(rng, 100));
    printf("\n");

    /* a number in a range: the bounds are included */
    proven_xoshiro256ss_seed(&g, 7);
    printf("dice        :");
    for (int i = 0; i < 8; i++) printf(" %lld", (long long)proven_rng_range(rng, 1, 6));
    printf("\n");

    /* randomness for secrets is not taken from here — the OS has its own source */
    proven_byte_t key[16];
    bool ok = proven_random_bytes(key, sizeof key);
    printf("os entropy  : %s (%zu bytes requested)\n", ok ? "available" : "unavailable", sizeof key);
    return 0;
}
