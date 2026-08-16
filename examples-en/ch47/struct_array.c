/* Arrays of structs and pointers to them - stride, tail padding, two ways to walk. */
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct rec {
    int  id;        /* 4 bytes, at a multiple of 4 */
    char code;      /* 1 byte */
                    /* <- 3 bytes of tail padding go here */
};

/* Taking an array of structs - it decays to a pointer, so the count comes too */
static int sum_ids(const struct rec *v, size_t n)
{
    int sum = 0;
    for (size_t i = 0; i < n; i++)
        sum += v[i].id;             /* v[i] is *(v + i) --- the stride is sizeof(struct rec) */
    return sum;
}

/* To change one element, take a pointer (by value only the copy changes) */
static void bump(struct rec *r) { r->id += 100; }
static void bump_by_value(struct rec r) { r.id += 100; }

int main(void)
{
    struct rec v[3] = { { 1, 'a' }, { 2, 'b' }, { 3, 'c' } };

    printf("[one element]\n");
    printf("  sizeof(struct rec) = %zu, alignof = %zu\n",
           sizeof(struct rec), alignof(struct rec));
    printf("  offsetof id = %zu, code = %zu -> tail padding = %zu bytes\n",
           offsetof(struct rec, id), offsetof(struct rec, code),
           sizeof(struct rec) - (offsetof(struct rec, code) + sizeof(char)));

    printf("\n[the array: the stride is exactly sizeof]\n");
    printf("  sizeof v = %zu (= %zu x %zu)\n",
           sizeof v, sizeof v / sizeof v[0], sizeof v[0]);
    for (size_t i = 0; i < 3; i++)
        printf("    &v[%zu] = %p   offset +%td\n", i, (const void *)&v[i],
               (const char *)&v[i] - (const char *)&v[0]);
    printf("  no gap: (char *)&v[1] - (char *)&v[0] == sizeof v[0] -> %s\n",
           ((const char *)&v[1] - (const char *)&v[0]) == (ptrdiff_t)sizeof v[0]
               ? "true" : "false");

    printf("\n[two ways to walk it]\n");
    int by_index = 0;
    for (size_t i = 0; i < 3; i++)
        by_index += v[i].id;
    int by_pointer = 0;
    for (const struct rec *p = v; p != v + 3; p++)   /* one past the end is legal */
        by_pointer += p->id;
    printf("  by index = %d, by pointer = %d, via function = %d\n",
           by_index, by_pointer, sum_ids(v, 3));

    printf("\n[changing one element]\n");
    bump_by_value(v[0]);
    printf("  after bump_by_value : v[0].id = %d (unchanged - it got a copy)\n", v[0].id);
    bump(&v[0]);
    printf("  after bump(&v[0])   : v[0].id = %d\n", v[0].id);

    printf("\n[why the tail padding matters at scale]\n");
    printf("  a million records: %zu bytes, of which padding is %zu\n",
           sizeof(struct rec) * 1000000u,
           (sizeof(struct rec) - (offsetof(struct rec, code) + 1)) * 1000000u);
    return 0;
}
