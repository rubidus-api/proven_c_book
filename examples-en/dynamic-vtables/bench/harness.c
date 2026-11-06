/* Sets up the objects and times the ways in ways.c.
   - The objects sit in one array and are always walked in the same order.
     Sorted and mixed differ only in the *order of the kinds*, so the difference
     between them comes from branch prediction, not from memory.
   - The baseline is "one kind, a direct call", timed again for every condition;
     every cell is a multiple of it.
   - The ways take turns, 41 times, and the shortest time is kept: other work on the
     machine can only make a run longer.
   ★ This program measures time. Only ratios are printed; they vary by machine and run.
   Run with "short" to print only the rows the mitigation runs compare. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bench.h"

#define ROUNDS 41

struct obj arena[N];
struct fat fats[N];
uint32_t *by_kind[KMAX], count[KMAX], kinds;
static struct ops *heap_ops[KMAX];
static struct method_entry method_lists[KMAX][8];
static struct klass classes[KMAX];

static uint64_t rng = 0x9E3779B97F4A7C15u;
static uint32_t rnd(uint32_t n)
{
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return (uint32_t)(rng % n);
}
static void setup(uint32_t k, bool mixed)
{
    static uint32_t seq[N];
    kinds = k;
    for (uint32_t i = 0; i < N; i++) seq[i] = (uint32_t)((uint64_t)i * k / N);
    if (mixed)                                              /* same kinds, shuffled */
        for (uint32_t i = N - 1; i > 0; i--) {
            uint32_t j = rnd(i + 1), t = seq[i];
            seq[i] = seq[j];
            seq[j] = t;
        }
    for (uint32_t j = 0; j < KMAX; j++) {
        count[j] = 0;
        for (uint32_t e = 0; e < 16; e++) classes[j].cache[e] = (struct cache_entry){};
    }
    for (uint32_t i = 0; i < N; i++) {
        uint32_t j = seq[i];
        arena[i].kind = j;
        arena[i].sops = &static_ops[j];
        arena[i].dops = heap_ops[j];
        arena[i].cls = &classes[j];
        fats[i] = (struct fat){ &arena[i], &static_ops[j] };
        by_kind[j][count[j]++] = i;
    }
}

static double seconds(void)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
static volatile uint64_t sink;
static double timed(uint64_t (*job)(void))
{
    double t = seconds();
    sink = job();
    return seconds() - t;
}

static const struct { const char *name; uint64_t (*job)(void); bool in_short; } ways[] = {
    { "one kind, direct call",    m_direct,    false },     /* the baseline */
    { "grouped by kind, direct",  m_grouped,   true },
    { "switch",                   m_switch,    true },
    { "binary branches",          m_binary,    true },
    { "static const table",       m_static,    true },
    { "table built at run time",  m_dynamic,   false },
    { "fat pointer",              m_fat,       false },
    { "selector, no cache",       m_sel_slow,  false },
    { "selector, class cache",    m_sel_cache, true },
    { "selector, inline cache",   m_sel_ic,    false },
    { "selector, 4-way PIC",      m_sel_pic,   false },
    { "switch, inlining allowed", m_inlined,   true },
};
#define WAYS (sizeof ways / sizeof ways[0])

int main(int argc, char **argv)
{
    bool brief = argc > 1 && strcmp(argv[1], "short") == 0;
    for (uint32_t j = 0; j < KMAX; j++) {
        by_kind[j] = malloc(N * sizeof *by_kind[j]);
        heap_ops[j] = malloc(sizeof *heap_ops[j]);
        if (!by_kind[j] || !heap_ops[j]) { puts("out of memory"); return 1; }
        *heap_ops[j] = (struct ops){ other, other };      /* defaults first */
        heap_ops[j]->apply = fs[j];                        /* then this kind's slot */
        for (uint32_t m = 0; m < 7; m++)
            method_lists[j][m] = (struct method_entry){ m + 1, other };
        method_lists[j][7] = (struct method_entry){ SEL_APPLY, fs[j] };
        classes[j] = (struct klass){ nullptr, method_lists[j], 8, {} };
    }
    for (uint32_t i = 0; i < N; i++) arena[i].x = i % 1000;

    static const uint32_t ks[] = { 1, 2, 8 };
    double ratio[WAYS][6];
    bool agree = true;
    for (int c = 0; c < 6; c++) {
        setup(ks[c / 2], c % 2);
        double best[WAYS];
        uint64_t sum[WAYS];
        for (size_t w = 0; w < WAYS; w++) {                /* warm up, keep the sum */
            sum[w] = ways[w].job();
            best[w] = 1e30;
        }
        for (int r = 0; r < ROUNDS; r++)                    /* the ways take turns */
            for (size_t w = 0; w < WAYS; w++) {
                double t = timed(ways[w].job);
                if (t < best[w]) best[w] = t;
            }
        for (size_t w = 0; w < WAYS; w++) {
            ratio[w][c] = best[w] / best[0];
            if (w > 1 && sum[w] != sum[1]) agree = false;
        }
        if (c == 0 && sum[0] != sum[1]) agree = false;   /* one kind: as the baseline */
    }

    printf("%u objects x %d passes; each cell is a multiple of one direct call\n",
           N, REPS);
    printf("%-25s %7s %7s %7s %7s %7s %7s\n", "kinds:", "1", "1", "2", "2", "8", "8");
    printf("%-25s %7s %7s %7s %7s %7s %7s\n", "order:",
           "sorted", "mixed", "sorted", "mixed", "sorted", "mixed");
    for (size_t w = 1; w < WAYS; w++) {
        if (brief && !ways[w].in_short) continue;
        printf("%-25s", ways[w].name);
        for (int c = 0; c < 6; c++) printf(" %7.2f", ratio[w][c]);
        putchar('\n');
    }
    printf("every way computed the same sum: %s\n", agree ? "yes" : "NO");
    for (uint32_t j = 0; j < KMAX; j++) { free(by_kind[j]); free(heap_ops[j]); }
    return agree ? 0 : 1;
}
