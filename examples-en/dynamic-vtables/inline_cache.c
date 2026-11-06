/* Inline caches and polymorphic inline caches --- a cache at each call site.
   Smalltalk and JIT runtimes rewrite the calling machine code itself. C cannot
   rewrite its own code, so the cache is a static variable beside the call and
   only that call uses it.
   - One entry (IC): one "last class -> function".
   - Four entries (PIC): four of them. A miss replaces the entries in turn.
   - Megamorphic: once a full PIC misses, the site stops comparing and goes to
     the per-class cache.
   Each entry records the generation number, for the same reason as lookup.c in
   chapter 72: a method changed at run time must not stay in the cache. */
#include <stdio.h>

struct object;
typedef double (*imp)(const struct object *self);

struct method { unsigned sel; imp fn; };
struct entry { const struct class *cls; imp fn; unsigned epoch; };
struct class {
    const struct class *parent;
    struct method methods[2];
    unsigned n_methods;
    double factor;
    struct entry cache;                  /* per-class cache, one selector here */
};
struct object { struct class *isa; double size; };

enum { SEL_AREA = 1 };

static unsigned epoch = 1;               /* rises whenever a method changes */
static unsigned slow_lookups;

static imp lookup_slow(const struct class *c, unsigned sel)
{
    slow_lookups++;
    for (; c; c = c->parent)
        for (unsigned i = 0; i < c->n_methods; i++)
            if (c->methods[i].sel == sel) return c->methods[i].fn;
    return nullptr;
}
static imp lookup_class_cache(struct class *c, unsigned sel)
{
    if (c->cache.epoch != epoch)
        c->cache = (struct entry){ c, lookup_slow(c, sel), epoch };
    return c->cache.fn;
}

/* ---- the cache of one call site ---- */
/* No selector is stored: the message sent from one site is always the same. */
struct site {
    struct entry e[4];
    unsigned ways, next;                 /* entries in use (1 or 4), next to replace */
    bool megamorphic;
    unsigned sends, misses;
};

static double send_at(struct site *s, struct object *o, unsigned sel)
{
    s->sends++;
    if (s->megamorphic)                  /* stopped comparing: per-class cache */
        return lookup_class_cache(o->isa, sel)(o);
    for (unsigned i = 0; i < s->ways; i++)
        if (s->e[i].cls == o->isa && s->e[i].epoch == epoch)
            return s->e[i].fn(o);        /* hit: one comparison per entry, no lookup */
    s->misses++;
    if (s->ways > 1 && s->e[s->next].cls && s->e[s->next].epoch == epoch) {
        s->megamorphic = true;           /* full and missed: give up comparing */
        return lookup_class_cache(o->isa, sel)(o);
    }
    imp fn = lookup_slow(o->isa, sel);
    s->e[s->next] = (struct entry){ o->isa, fn, epoch };
    s->next = (s->next + 1) % s->ways;
    return fn(o);
}

static const char *state(const struct site *s)
{
    if (s->megamorphic) return "megamorphic";
    unsigned used = 0;
    for (unsigned i = 0; i < s->ways; i++) used += s->e[i].epoch == epoch;
    return used == 0 ? "empty" : used == 1 ? "monomorphic" : "polymorphic";
}
static void report(const char *label, struct site *s, double sum)
{
    printf("  %-12s sum %5g, %2u sends, %2u misses, %-11s (slow lookups %u)\n",
           label, sum, s->sends, s->misses, state(s), slow_lookups);
    s->sends = s->misses = 0;
    slow_lookups = 0;
}

/* Two call sites. Each block-scope static is the cache of the one call below it. */
static void total_area_ic(struct object *xs, size_t n)
{
    static struct site here = { .ways = 1 };   /* this call site's own cache */
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += send_at(&here, &xs[i], SEL_AREA);
    report("1-entry IC", &here, sum);
}
static void total_area_pic(struct object *xs, size_t n)
{
    static struct site here = { .ways = 4 };   /* and this one's */
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += send_at(&here, &xs[i], SEL_AREA);
    report("4-entry PIC", &here, sum);
}

/* ---- classes: every shape's area is factor x size x size ---- */
static double scaled_area(const struct object *self)
{ return self->isa->factor * self->size * self->size; }
static double circle_area(const struct object *self)
{ return 3 * self->size * self->size; }
static double circle_area_2(const struct object *self)      /* added at run time */
{ return 3.14 * self->size * self->size; }

static struct class shape = {
    .methods = { { SEL_AREA, scaled_area } }, .n_methods = 1, .factor = 1 };
static struct class circle = {
    .parent = &shape, .methods = { { SEL_AREA, circle_area } }, .n_methods = 1 };
#define KIND(var, f) static struct class var = { .parent = &shape, .factor = f }
KIND(square, 1); KIND(triangle, 0.5); KIND(hexagon, 2.5);
KIND(kite, 0.5); KIND(star, 4);

static void scene(const char *title, struct object *xs, size_t n)
{
    puts(title);
    total_area_ic(xs, n);
    total_area_pic(xs, n);
}

int main(void)
{
    struct object same[8], two[8], six[12];
    struct class *six_kinds[] = { &circle, &square, &triangle, &hexagon, &kite, &star };
    for (size_t i = 0; i < 8; i++) {
        same[i] = (struct object){ &circle, 1 };
        two[i] = (struct object){ i % 2 ? &square : &circle, 1 };
    }
    for (size_t i = 0; i < 12; i++) six[i] = (struct object){ six_kinds[i % 6], 1 };

    scene("one class only (8 circles):", same, 8);
    scene("two classes taking turns (circle, square, ...):", two, 8);
    scene("two classes again, after the first round:", two, 8);
    scene("six classes taking turns:", six, 12);

    puts("change Circle's area at run time (generation rises), then two classes:");
    circle.methods[0].fn = circle_area_2;
    epoch++;
    total_area_ic(two, 8);
    total_area_pic(two, 8);
    return 0;
}
