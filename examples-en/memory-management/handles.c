/* Handles with generation numbers --- references that *can detect* dangling.
 * Hand out (slot number, generation) instead of a pointer. Each return of a slot raises its generation,
 * so an old handle no longer matches and is judged "an object already dead". The slot may be reused. */
#include <stdint.h>
#include <stdio.h>

enum { SLOTS = 4 };

typedef struct { uint32_t index, gen; } handle;

typedef struct {
    double   value[SLOTS];
    uint32_t gen[SLOTS];        /* generation per slot */
    bool     used[SLOTS];
} pool;

static bool pool_new(pool *p, double v, handle *out)
{
    for (uint32_t i = 0; i < SLOTS; i++) {
        if (!p->used[i]) {
            p->used[i] = true;
            p->value[i] = v;
            *out = (handle){ i, p->gen[i] };
            return true;
        }
    }
    return false;               /* no free slot */
}

static void pool_free(pool *p, handle h)
{
    if (h.index < SLOTS && p->used[h.index] && p->gen[h.index] == h.gen) {
        p->used[h.index] = false;
        p->gen[h.index]++;      /* ★ every handle to this slot goes stale at this moment */
    }
}

/* Return the place if the object is alive, otherwise NULL --- you *can ask* before using it. */
static double *pool_get(pool *p, handle h)
{
    if (h.index >= SLOTS || !p->used[h.index] || p->gen[h.index] != h.gen)
        return NULL;
    return &p->value[h.index];
}

static void show(pool *p, const char *name, handle h)
{
    double *v = pool_get(p, h);
    if (v) printf("%-6s (slot %u, gen %u) -> %.1f\n", name, (unsigned)h.index, (unsigned)h.gen, *v);
    else   printf("%-6s (slot %u, gen %u) -> stale handle, object is gone\n",
                  name, (unsigned)h.index, (unsigned)h.gen);
}

int main(void)
{
    pool p = {0};
    handle a, b;
    if (!pool_new(&p, 1.5, &a)) return 1;
    show(&p, "a", a);

    pool_free(&p, a);               /* return the object a referred to */
    if (!pool_new(&p, 9.0, &b)) return 1;  /* the same slot is reused */
    show(&p, "b", b);
    show(&p, "a", a);               /* as a pointer, this would have read b's value */
    return 0;
}
