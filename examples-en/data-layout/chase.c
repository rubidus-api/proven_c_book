/* Summing the same million numbers three ways: an array, a linked list whose nodes
   happen to lie in order, and the same list after the heap has been "used for a while"
   (nodes linked in a shuffled order).
   ★ This program measures time; it prints ratios only. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N      (1u << 20)
#define ROUNDS 5

struct node { struct node *next; uint32_t value; };     /* the list */
static uint32_t *arr;                                    /* the array */
static struct node *pool, *in_order, *shuffled;
static volatile uint64_t sink;

static double now(void) { return (double)clock() / CLOCKS_PER_SEC; }
static double sum_array(void)
{
    double t = now(); uint64_t s = 0;
    for (uint32_t i = 0; i < N; i++) s += arr[i];
    sink = s; return now() - t;
}
static double walk(const struct node *p)
{
    double t = now(); uint64_t s = 0;
    for (; p; p = p->next) s += p->value;
    sink = s; return now() - t;
}
static double walk_in_order(void) { return walk(in_order); }
static double walk_shuffled(void) { return walk(shuffled); }

static int cmp(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static double median(double (*job)(void))
{
    double r[ROUNDS];
    job();
    for (int i = 0; i < ROUNDS; i++) r[i] = job();
    qsort(r, ROUNDS, sizeof *r, cmp);
    return r[ROUNDS / 2];
}

static struct node *make_list(uint32_t *order)
{
    for (uint32_t k = 0; k + 1 < N; k++) pool[order[k]].next = &pool[order[k + 1]];
    pool[order[N - 1]].next = nullptr;
    return &pool[order[0]];
}

int main(void)
{
    arr = malloc(N * sizeof *arr);
    pool = malloc(N * sizeof *pool);
    uint32_t *order = malloc(N * sizeof *order);
    if (!arr || !pool || !order) { puts("out of memory"); return 1; }
    for (uint32_t i = 0; i < N; i++) { arr[i] = i; pool[i].value = i; order[i] = i; }

    in_order = make_list(order);
    double a = median(sum_array), l = median(walk_in_order);

    /* shuffle the links, not the nodes */
    uint64_t rng = 0x2545F4914F6CDD1Du;
    for (uint32_t i = N - 1; i > 0; i--) {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        uint32_t j = (uint32_t)(rng >> 32) % (i + 1), t = order[i];
        order[i] = order[j]; order[j] = t;
    }
    shuffled = make_list(order);
    double s = median(walk_shuffled);

    printf("%u numbers; a node is %zu bytes, a number %zu\n\n",
           N, sizeof *pool, sizeof *arr);
    if (a > 0) {
        printf("list, nodes in memory order : %5.1f times as long as the array\n", l / a);
        printf("list, nodes scattered       : %5.1f times as long as the array\n", s / a);
    } else puts("the clock is too coarse on this machine to tell them apart");
    puts("\n(same nodes, same values; only where the next one lives has changed.)");
    free(arr); free(pool); free(order);
    return 0;
}
