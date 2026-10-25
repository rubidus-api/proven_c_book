/* 열린 주소법 해시 표는 캐시에 유리하다: 배열 하나에서 이웃 칸을 차례로 본다.
   그 대신 해시만큼만 좋다. 부딪히도록 고른 열쇠를 넣으면 넣을 때마다 덩어리 전체를
   훑는다. 아래의 수는 시간이 아니라 탐색 횟수다 --- 실행마다 같다. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SLOTS (1u << 16)
#define KEYS  4000u

static uint64_t table[SLOTS];
static bool used[SLOTS];

/* 누구나 짐작할 수 있는 해시: 열쇠의 아래 비트 */
static uint32_t hash_plain(uint64_t k, uint64_t seed) { (void)seed; return (uint32_t)k; }

/* 열쇠를 넣은 섞기: 바깥은 `seed` 를 모른다. (보여 주기용이다 --- 네트워크를 마주한
   실제 표는 이 목적으로 설계된 해시를 쓴다. 흔히 SipHash 다.) */
static uint32_t hash_keyed(uint64_t k, uint64_t seed)
{
    k ^= seed;
    k ^= k >> 33; k *= 0xFF51AFD7ED558CCDu;
    k ^= k >> 33; k *= 0xC4CEB9FE1A85EC53u;
    k ^= k >> 33;
    return (uint32_t)k;
}

static unsigned long insert_all(const uint64_t *keys,
                                uint32_t (*h)(uint64_t, uint64_t), uint64_t seed)
{
    memset(used, 0, sizeof used);
    unsigned long probes = 0;
    for (uint32_t i = 0; i < KEYS; i++) {
        uint32_t s = h(keys[i], seed) & (SLOTS - 1);
        while (used[s]) { probes++; s = (s + 1) & (SLOTS - 1); }   /* 선형 탐사 */
        used[s] = true; table[s] = keys[i];
        probes++;
    }
    return probes;
}

int main(void)
{
    static uint64_t ordinary[KEYS], crafted[KEYS];
    uint64_t rng = 0x853C49E6748FEA9Bu;
    for (uint32_t i = 0; i < KEYS; i++) {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        ordinary[i] = rng;
        crafted[i]  = (uint64_t)i << 16;          /* 모두 아래 16비트가 같다 */
    }
    /* 실제 프로그램에서는 프로세스마다 무작위로 */
    uint64_t seed = 0x5DEECE66Du;

    printf("%u keys into %u slots, linear probing\n\n", KEYS, SLOTS);
    printf("plain hash, ordinary keys : %8lu probes\n",
           insert_all(ordinary, hash_plain, seed));
    printf("plain hash, crafted keys  : %8lu probes\n",
           insert_all(crafted, hash_plain, seed));
    printf("keyed hash, crafted keys  : %8lu probes\n",
           insert_all(crafted, hash_keyed, seed));
    puts("\n(n keys that all collide cost about n*n/2 probes:");
    puts(" the work grows with the square.)");
    return 0;
}
