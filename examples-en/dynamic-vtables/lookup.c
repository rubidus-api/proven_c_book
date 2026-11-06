/* Dispatch by name (selector) --- the Smalltalk and Objective-C way, in C.
   1. A message name is interned once and becomes an integer selector.
   2. The class's method list is scanned; if the method is not there, the parent's.
   3. What is found goes into a per-class cache; next time the cache answers.
   4. If nothing is found, the "does not understand" handler gets it (forwarding).
   5. Adding a method at run time bumps an epoch, which throws away every cache. */
#include <stdio.h>
#include <string.h>

#define MAX_SELECTORS 32
#define MAX_METHODS   8
#define CACHE_SLOTS   8

struct object;
typedef double (*imp)(struct object *self);

struct method { unsigned sel; imp fn; };
struct cache_slot { unsigned sel; imp fn; unsigned epoch; };
struct class {
    const char *name;
    struct class *parent;
    struct method methods[MAX_METHODS];
    unsigned n_methods;
    struct cache_slot cache[CACHE_SLOTS];
};
struct object { struct class *isa; double size; };

/* ---- interning selectors ---- */
static const char *selector_names[MAX_SELECTORS];
static unsigned n_selectors;
static unsigned intern(const char *name)
{
    for (unsigned i = 0; i < n_selectors; i++)
        if (strcmp(selector_names[i], name) == 0) return i + 1;
    if (n_selectors == MAX_SELECTORS) return 0;
    selector_names[n_selectors++] = name;
    return n_selectors;                       /* 0 is kept for "no selector" */
}

/* ---- lookup ---- */
static unsigned epoch = 1;                    /* goes up whenever methods change */
static unsigned slow_lookups, cache_hits;

static imp lookup_slow(const struct class *c, unsigned sel)
{
    slow_lookups++;
    for (; c; c = c->parent)
        for (unsigned i = 0; i < c->n_methods; i++)
            if (c->methods[i].sel == sel) return c->methods[i].fn;
    return nullptr;
}
static imp lookup(struct class *c, unsigned sel)
{
    struct cache_slot *slot = &c->cache[sel % CACHE_SLOTS];
    if (slot->sel == sel && slot->epoch == epoch) { cache_hits++; return slot->fn; }
    imp fn = lookup_slow(c, sel);
    *slot = (struct cache_slot){ sel, fn, epoch };     /* "not found" is cached too */
    return fn;
}
static bool add_method(struct class *c, const char *name, imp fn)
{
    if (c->n_methods == MAX_METHODS) return false;
    c->methods[c->n_methods++] = (struct method){ intern(name), fn };
    epoch++;                                  /* every class's cache is stale at once */
    return true;
}

/* ---- sending ---- */
static double does_not_understand(struct object *self, unsigned sel)
{
    printf("  %s does not understand '%s'\n", self->isa->name,
           sel ? selector_names[sel - 1] : "(selector table full)");
    return 0;
}
static double send(struct object *self, const char *message)
{
    unsigned sel = intern(message);
    imp fn = sel ? lookup(self->isa, sel) : nullptr;
    return fn ? fn(self) : does_not_understand(self, sel);
}

/* ---- the classes ---- */
static double shape_describe(struct object *self)
{ printf("  a %s of size %g\n", self->isa->name, self->size); return 0; }
static double circle_area(struct object *self) { return 3.0 * self->size * self->size; }
static double square_area(struct object *self) { return self->size * self->size; }
static double shape_perimeter(struct object *self) { return 4.0 * self->size; }

static struct class shape  = { .name = "Shape" };
static struct class circle = { .name = "Circle", .parent = &shape };
static struct class square = { .name = "Square", .parent = &shape };

int main(void)
{
    add_method(&shape, "describe", shape_describe);
    add_method(&circle, "area", circle_area);
    add_method(&square, "area", square_area);

    struct object things[] = { { &circle, 2 }, { &square, 3 }, { &circle, 1 } };
    puts("send 'describe' (inherited) and 'area' (own) to each:");
    for (size_t i = 0; i < sizeof things / sizeof things[0]; i++) {
        send(&things[i], "describe");
        printf("  area = %g\n", send(&things[i], "area"));
    }
    printf("slow lookups %u, cache hits %u\n\n", slow_lookups, cache_hits);

    puts("send 'perimeter', which nobody defines yet:");
    send(&things[0], "perimeter");
    send(&things[0], "perimeter");            /* "not found" comes from the cache too */
    printf("slow lookups %u, cache hits %u\n\n", slow_lookups, cache_hits);

    puts("add 'perimeter' to Shape at run time, then send again:");
    add_method(&shape, "perimeter", shape_perimeter);
    printf("  perimeter = %g\n", send(&things[0], "perimeter"));
    printf("  perimeter = %g\n", send(&things[1], "perimeter"));
    printf("slow lookups %u, cache hits %u\n", slow_lookups, cache_hits);
    return 0;
}
