/* A toy mark-sweep collector --- life and death decided by "is it reachable from the roots?".
 * Even a cycle reference counting could not reclaim is reclaimed once cut off from the roots. Unlike a real
 * collector, roots are registered by hand --- C has no ledger saying "where the pointers are". */
#include <stdio.h>
#include <stdlib.h>

typedef struct obj obj;
struct obj {
    char  name;
    obj  *ref[2];       /* two places that point at other objects */
    bool  marked;
    int   survived;     /* how many collections it survived */
    obj  *next_alloc;   /* list of every object the collector knows */
};

static obj *all;                /* every allocated object */
static obj *roots[4];           /* roots: what the program holds right now */

static obj *gc_new(char name)
{
    obj *o = calloc(1, sizeof *o);
    if (o == NULL) exit(1);
    o->name = name;
    o->next_alloc = all;
    all = o;
    return o;
}

static void mark(obj *o)
{
    obj *stack[64];             /* an explicit stack instead of recursion --- so deep graphs do not overflow the call stack */
    size_t top = 0;
    if (o) stack[top++] = o;
    while (top > 0) {
        obj *x = stack[--top];
        if (x->marked) continue;        /* marks each object once, even in a cycle */
        x->marked = true;
        for (int i = 0; i < 2; i++)
            if (x->ref[i] && !x->ref[i]->marked && top < sizeof stack / sizeof stack[0])
                stack[top++] = x->ref[i];
    }
}

static void collect(void)
{
    for (size_t i = 0; i < sizeof roots / sizeof roots[0]; i++)
        mark(roots[i]);                 /* (1) mark: what the roots reach */

    printf("  freed:");
    for (obj **p = &all; *p != NULL; ) { /* (2) sweep: return what has no mark */
        obj *o = *p;
        if (!o->marked) {
            *p = o->next_alloc;
            printf(" %c", o->name);
            free(o);
        } else {
            o->marked = false;          /* clear for the next collection */
            o->survived++;
            p = &o->next_alloc;
        }
    }
    printf("\n  alive:");
    for (obj *o = all; o != NULL; o = o->next_alloc)
        printf(" %c(survived %d)", o->name, o->survived);
    printf("\n");
}

int main(void)
{
    obj *a = gc_new('a'), *b = gc_new('b'), *c = gc_new('c');
    a->ref[0] = b;
    b->ref[0] = c;
    c->ref[0] = b;                      /* b <-> c cycle */
    roots[0] = a;
    (void)gc_new('t');                  /* a temporary nobody holds */

    puts("collection 1 (root: a):");
    collect();

    a->ref[0] = NULL;                   /* cut the road from a to the cycle */
    (void)gc_new('u');
    puts("collection 2 (a no longer points at the cycle):");
    collect();

    roots[0] = NULL;                    /* empty the roots, final collection */
    puts("collection 3 (no roots):");
    collect();
    return 0;
}
