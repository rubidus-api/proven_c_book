/* One table per (interface, concrete type) pair, built at run time on first use and
   kept in a hash table --- the idea of Go's itab, in C.
   A type only lists its methods by name; it never says which interfaces it satisfies.
   A failed match is cached too, so asking again costs a lookup, not a rebuild. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef double (*fn)(const void *self);
struct method { const char *name; fn f; };
struct type  { const char *name; const struct method *methods; size_t n; };
struct iface { const char *name; const char *const *wants; size_t n; };
struct itab  { const struct iface *in; const struct type *ty; bool ok; fn fun[4]; };

/* ---- the cache: open addressing, keyed by the pair of pointers ---- */
enum { SLOTS = 64 };
static struct itab *cache[SLOTS];
static unsigned builds, hits;

static size_t hash(const struct iface *in, const struct type *ty)
{
    uintptr_t h = (uintptr_t)in * 31 ^ (uintptr_t)ty;
    return (size_t)(h ^ h >> 7) % SLOTS;
}
static struct itab *build(const struct iface *in, const struct type *ty)
{
    struct itab *t = calloc(1, sizeof *t);
    if (!t) return nullptr;
    *t = (struct itab){ in, ty, true, {} };
    builds++;
    for (size_t w = 0; w < in->n && w < 4; w++) {          /* match by name, in order */
        fn found = nullptr;
        for (size_t m = 0; m < ty->n; m++)
            if (strcmp(ty->methods[m].name, in->wants[w]) == 0) found = ty->methods[m].f;
        if (!found) { t->ok = false; break; }
        t->fun[w] = found;
    }
    return t;
}
static const struct itab *get_itab(const struct iface *in, const struct type *ty)
{
    size_t i = hash(in, ty);
    for (size_t probes = 0; probes < SLOTS; probes++, i = (i + 1) % SLOTS) {
        if (!cache[i]) return cache[i] = build(in, ty);   /* first time: build, keep */
        if (cache[i]->in == in && cache[i]->ty == ty) { hits++; return cache[i]; }
    }
    return nullptr;                                         /* cache full */
}

/* ---- types that know nothing about interfaces ---- */
struct circle { double r; };
struct label  { const char *text; };
static double circle_area(const void *s)
{ const struct circle *c = s; return 3.0 * c->r * c->r; }
static double circle_perimeter(const void *s)
{ const struct circle *c = s; return 6.0 * c->r; }
static double label_length(const void *s)
{ const struct label *l = s; return (double)strlen(l->text); }

static const struct method circle_m[] = {
    { "area", circle_area }, { "perimeter", circle_perimeter },
};
static const struct method label_m[]  = { { "length", label_length } };
static const struct type circle_t = { "circle", circle_m, 2 };
static const struct type label_t  = { "label", label_m, 1 };

static const char *const shape_wants[] = { "area", "perimeter" };
static const struct iface shape_i = { "Shape", shape_wants, 2 };

/* an interface value: the itab plus the data --- a fat pointer */
struct shape_value { const struct itab *tab; const void *data; };
static bool as_shape(const struct type *ty, const void *data, struct shape_value *out)
{
    const struct itab *t = get_itab(&shape_i, ty);
    if (!t || !t->ok) return false;
    *out = (struct shape_value){ t, data };
    return true;
}

int main(void)
{
    struct circle cs[] = { { 1 }, { 2 }, { 0.5 } };
    struct label lb = { "hello" };
    for (size_t i = 0; i < 3; i++) {
        struct shape_value v;
        if (as_shape(&circle_t, &cs[i], &v))
            printf("circle %.1f as Shape: area %.2f, perimeter %.2f\n",
                   cs[i].r, v.tab->fun[0](v.data), v.tab->fun[1](v.data));
    }
    for (int i = 0; i < 2; i++) {
        struct shape_value v;
        bool ok = as_shape(&label_t, &lb, &v);
        printf("label as Shape: %s\n", ok ? "yes" : "no --- it has no area");
    }
    printf("tables built %u (one per pair, the failure too), cache hits %u\n",
           builds, hits);
    for (size_t i = 0; i < SLOTS; i++) free(cache[i]);
    return 0;
}
