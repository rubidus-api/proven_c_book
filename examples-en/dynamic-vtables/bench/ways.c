/* The ways of choosing a function, timed by harness.c.
   - Every kind has its own function (different constants, so they cannot be merged),
     and none may be inlined: what is measured is the choosing, not the work.
   - The one exception is the last way, which lets the compiler inline. */
#include "bench.h"

#define KIND(k, c)                                                              \
    [[gnu::noinline]] static uint64_t f##k(const struct obj *o)                 \
    { return o->x * (c) + (k); }                                                \
    static inline uint64_t g##k(const struct obj *o) { return o->x * (c) + (k); }
KIND(0, 3)  KIND(1, 5)  KIND(2, 7)  KIND(3, 11)
KIND(4, 13) KIND(5, 17) KIND(6, 19) KIND(7, 23)

[[gnu::noinline]] uint64_t other(const struct obj *o) { return o->x; }
const method fs[KMAX] = { f0, f1, f2, f3, f4, f5, f6, f7 };
const struct ops static_ops[KMAX] = {
    { f0, other }, { f1, other }, { f2, other }, { f3, other },
    { f4, other }, { f5, other }, { f6, other }, { f7, other },
};

#define EACH for (uint32_t r = 0; r < REPS; r++) for (uint32_t i = 0; i < N; i++)

uint64_t m_direct(void)          /* the baseline: one kind, a direct call */
{ uint64_t s = 0; EACH s += f0(&arena[i]); return s; }

uint64_t m_grouped(void)         /* grouped by kind (an index list per kind), direct */
{
    uint64_t s = 0;
    for (uint32_t r = 0; r < REPS; r++)
        for (uint32_t k = 0; k < kinds; k++) {
            const uint32_t *ix = by_kind[k];
            switch (k) {
#define GROUP(j) \
    case j: for (uint32_t i = 0; i < count[j]; i++) s += f##j(&arena[ix[i]]); break;
            GROUP(0) GROUP(1) GROUP(2) GROUP(3) GROUP(4) GROUP(5) GROUP(6) GROUP(7)
            }
        }
    return s;
}

uint64_t m_switch(void)          /* switch on the kind number */
{
    uint64_t s = 0;
    EACH {
        const struct obj *o = &arena[i];
        switch (o->kind) {
        case 0: s += f0(o); break; case 1: s += f1(o); break;
        case 2: s += f2(o); break; case 3: s += f3(o); break;
        case 4: s += f4(o); break; case 5: s += f5(o); break;
        case 6: s += f6(o); break; case 7: s += f7(o); break;
        }
    }
    return s;
}

uint64_t m_binary(void)          /* binary branches on the kind number, no table */
{
    uint64_t s = 0;
    EACH {
        const struct obj *o = &arena[i];
        uint32_t k = o->kind;
        if (k < 4) s += k < 2 ? (k < 1 ? f0(o) : f1(o)) : (k < 3 ? f2(o) : f3(o));
        else       s += k < 6 ? (k < 5 ? f4(o) : f5(o)) : (k < 7 ? f6(o) : f7(o));
    }
    return s;
}

uint64_t m_static(void)          /* a static const table */
{ uint64_t s = 0; EACH s += arena[i].sops->apply(&arena[i]); return s; }

uint64_t m_dynamic(void)         /* a table built at run time */
{ uint64_t s = 0; EACH s += arena[i].dops->apply(&arena[i]); return s; }

uint64_t m_fat(void)             /* a fat pointer */
{ uint64_t s = 0; EACH s += fats[i].ops->apply(fats[i].data); return s; }

/* ---- by selector: what Objective-C does, with and without caches ---- */
/* scan the list, then the parent's */
static method lookup_slow(const struct klass *c, uint32_t sel)
{
    for (; c; c = c->parent)
        for (uint32_t m = 0; m < c->n; m++)
            if (c->methods[m].sel == sel) return c->methods[m].fn;
    return nullptr;
}
/* the per-class cache first */
static method lookup_cached(struct klass *c, uint32_t sel)
{
    struct cache_entry *e = &c->cache[sel & 15];
    if (e->sel == sel) return e->fn;
    method m = lookup_slow(c, sel);
    *e = (struct cache_entry){ sel, m };
    return m;
}

uint64_t m_sel_slow(void)        /* by selector, no cache: scan the list to its end */
{ uint64_t s = 0; EACH s += lookup_slow(arena[i].cls, SEL_APPLY)(&arena[i]); return s; }

uint64_t m_sel_cache(void)       /* by selector, per-class cache */
{ uint64_t s = 0; EACH s += lookup_cached(arena[i].cls, SEL_APPLY)(&arena[i]); return s; }

uint64_t m_sel_ic(void)          /* by selector, one-entry inline cache at the call */
{
    uint64_t s = 0;
    struct { const struct klass *cls; method fn; } ic = {};
    EACH {
        struct klass *c = arena[i].cls;
        if (c != ic.cls) { ic.cls = c; ic.fn = lookup_slow(c, SEL_APPLY); }   /* miss */
        s += ic.fn(&arena[i]);
    }
    return s;
}

uint64_t m_sel_pic(void)         /* by selector, a four-entry polymorphic inline cache */
{
    uint64_t s = 0;
    struct { const struct klass *cls; method fn; } pic[4] = {};
    uint32_t next = 0;
    EACH {
        struct klass *c = arena[i].cls;
        method m;
        if      (pic[0].cls == c) m = pic[0].fn;
        else if (pic[1].cls == c) m = pic[1].fn;
        else if (pic[2].cls == c) m = pic[2].fn;
        else if (pic[3].cls == c) m = pic[3].fn;
        else {                                       /* miss: look up, replace one */
            m = lookup_slow(c, SEL_APPLY);
            pic[next].cls = c;
            pic[next].fn = m;
            next = (next + 1) & 3;
        }
        s += m(&arena[i]);
    }
    return s;
}

uint64_t m_inlined(void)         /* switch again, but the bodies may be inlined */
{
    uint64_t s = 0;
    EACH {
        const struct obj *o = &arena[i];
        switch (o->kind) {
        case 0: s += g0(o); break; case 1: s += g1(o); break;
        case 2: s += g2(o); break; case 3: s += g3(o); break;
        case 4: s += g4(o); break; case 5: s += g5(o); break;
        case 6: s += g6(o); break; case 7: s += g7(o); break;
        }
    }
    return s;
}
