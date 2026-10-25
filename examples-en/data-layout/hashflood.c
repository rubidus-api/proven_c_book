/* An open-addressing hash table is cache-friendly: one array, neighbours probed in order.
   It is also only as good as its hash. Feed it keys chosen to collide and every insert
   walks the whole cluster. The counts below are probes, not times --- they never vary. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SLOTS (1u << 16)
#define KEYS  4000u

static uint64_t table[SLOTS];
static bool used[SLOTS];

/* a hash anyone can predict: the low bits of the key */
static uint32_t hash_plain(uint64_t k, uint64_t seed) { (void)seed; return (uint32_t)k; }

/* a keyed mix: the outsider does not know `seed`. (Illustration only --- real tables
   facing the network use a keyed hash designed for it, SipHash being the usual one.) */
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
        while (used[s]) { probes++; s = (s + 1) & (SLOTS - 1); }   /* linear probing */
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
        crafted[i]  = (uint64_t)i << 16;          /* all share the same low 16 bits */
    }
    /* in a real program: random, per process */
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
