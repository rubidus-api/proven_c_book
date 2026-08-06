#include <proven.h>
#include <stdio.h>

int main(void)
{
    /* 재현 가능한 난수: 같은 씨앗 = 같은 수열. 시험과 시뮬레이션용이다 */
    proven_xoshiro256ss_t g;
    proven_xoshiro256ss_seed(&g, 12345);
    proven_rng_t rng = proven_xoshiro256ss_rng(&g);

    printf("seeded run 1:");
    for (int i = 0; i < 5; i++) printf(" %llu", (unsigned long long)proven_rng_below(rng, 100));
    printf("\n");

    proven_xoshiro256ss_seed(&g, 12345);          /* 같은 씨앗으로 되감는다 */
    printf("seeded run 2:");
    for (int i = 0; i < 5; i++) printf(" %llu", (unsigned long long)proven_rng_below(rng, 100));
    printf("\n");

    /* 범위 난수: 경계를 포함한다 */
    proven_xoshiro256ss_seed(&g, 7);
    printf("dice        :");
    for (int i = 0; i < 8; i++) printf(" %lld", (long long)proven_rng_range(rng, 1, 6));
    printf("\n");

    /* 비밀에 쓸 난수는 여기서 얻지 않는다 — OS 난수원이 따로 있다 */
    proven_byte_t key[16];
    bool ok = proven_random_bytes(key, sizeof key);
    printf("os entropy  : %s (%zu bytes requested)\n", ok ? "available" : "unavailable", sizeof key);
    return 0;
}
