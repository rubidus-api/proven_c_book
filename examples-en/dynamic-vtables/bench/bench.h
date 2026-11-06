/* Shared by ways.c (the ways of choosing a function) and harness.c (set-up, timing). */
#include <stdint.h>

#define N    (1u << 16)          /* objects: a 2 MiB arena, inside a large cache */
#define REPS 8                   /* passes over the arena per timed run */
#define KMAX 8

struct obj;
typedef uint64_t (*method)(const struct obj *);

struct ops { method apply; method other; };                /* a virtual table */
struct cache_entry { uint32_t sel; method fn; };
struct method_entry { uint32_t sel; method fn; };
struct klass {                                        /* a class found by selector */
    const struct klass *parent;
    const struct method_entry *methods;
    uint32_t n;
    struct cache_entry cache[16];                           /* per-class cache */
};
struct obj {
    uint64_t x;
    uint32_t kind;
    const struct ops *sops;      /* points to a static const table */
    const struct ops *dops;      /* points to a table built on the heap at run time */
    struct klass *cls;
};
struct fat { const struct obj *data; const struct ops *ops; };   /* fat pointer */

enum { SEL_APPLY = 8 };          /* selectors 1-7 are other methods; apply is last */

extern struct obj arena[N];
extern struct fat fats[N];
extern uint32_t *by_kind[KMAX], count[KMAX], kinds;
extern const method fs[KMAX];
extern const struct ops static_ops[KMAX];
uint64_t other(const struct obj *o);

uint64_t m_direct(void), m_grouped(void), m_switch(void), m_binary(void), m_static(void),
         m_dynamic(void), m_fat(void), m_inlined(void), m_sel_slow(void),
         m_sel_cache(void), m_sel_ic(void), m_sel_pic(void);
