/* Reference counting --- count "how many point at me" and return the object the moment it reaches 0.
 * And its weakness: two objects pointing at each other (a cycle) never reach 0.
 * Make one side a *weak reference* (one that does not raise the count) and the cycle comes apart. */
#include <stdio.h>
#include <stdlib.h>

typedef struct node node;
struct node {
    const char *name;
    int   refs;
    node *strong;       /* a reference that raises the count */
    node *weak;         /* a reference that does not raise the count --- not an owner */
};

static int live;        /* nodes not yet returned */

static node *node_new(const char *name)
{
    node *n = calloc(1, sizeof *n);
    if (n == NULL) exit(1);
    n->name = name;
    n->refs = 1;        /* the creator holds the first reference */
    live++;
    return n;
}

static node *retain(node *n) { if (n) n->refs++; return n; }

static void release(node *n)
{
    if (n == NULL || --n->refs > 0) return;
    printf("  free %s\n", n->name);
    release(n->strong);             /* drop the strong reference I hold */
    free(n);
    live--;
}

int main(void)
{
    puts("cycle of two strong references:");
    node *a = node_new("a"), *b = node_new("b");
    a->strong = retain(b);
    b->strong = retain(a);          /* a -> b -> a */
    release(a);
    release(b);                     /* every outside reference has been dropped */
    printf("  still alive: %d (a.refs %d, b.refs %d) --- leaked\n", live, a->refs, b->refs);

    /* The demonstration is over, so break the cycle by hand --- without a collector, this job is a person's. */
    a->strong = NULL; b->strong = NULL;
    free(a); free(b); live -= 2;

    puts("same shape, back edge made weak:");
    node *c = node_new("c"), *d = node_new("d");
    c->strong = retain(d);
    d->weak = c;                    /* d -> c does not raise the count */
    release(c);
    release(d);
    printf("  still alive: %d\n", live);
    return 0;
}
