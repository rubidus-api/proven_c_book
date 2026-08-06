/* Zeroing a whole struct — { 0 } and C23's { } null out pointer members too. */
#include <stdio.h>
#include <string.h>

struct inner { int k; char *note; };

struct config {
    int          retries;
    char        *path;      /* a pointer member */
    double       ratio;
    struct inner in;        /* which contains another pointer */
    char         name[4];
};

static void dump(const char *tag, const struct config *c)
{
    printf("%s retries=%d path=%s ratio=%g in.k=%d in.note=%s name[0]=%d\n",
           tag, c->retries,
           c->path == NULL ? "null" : "not null",
           c->ratio, c->in.k,
           c->in.note == NULL ? "null" : "not null",
           c->name[0]);
}

static void bytes(const char *tag, const void *p, size_t n)
{
    const unsigned char *b = p;
    size_t zero = 0;
    for (size_t i = 0; i < n; i++)
        zero += (b[i] == 0);
    printf("%s %zu of %zu bytes are zero\n", tag, zero, n);
}

int main(void)
{
    struct config a = {0};      /* only the first member is spelled out */
    struct config b = {};       /* C23: empty initializer — whole object */

    dump("{0} :", &a);
    dump("{ } :", &b);

    /* Designated initializers behave the same: what you leave out is
       default-initialized. */
    struct config c = { .retries = 3 };
    dump("{.retries=3}:", &c);

    /* memset writes all-bits-zero, which is not the same promise as "null" —
       the same here, but the standard does not guarantee it. */
    struct config m;
    memset(&m, 0, sizeof m);
    printf("after memset, is path null? %s (on this implementation)\n",
           m.path == NULL ? "yes" : "no");

    printf("\nsizeof(struct config) = %zu, sum of member sizes = %zu"
           " — the difference is padding\n",
           sizeof(struct config),
           sizeof(int) + sizeof(char *) + sizeof(double)
           + sizeof(struct inner) + 4);
    bytes("{0} :", &a, sizeof a);
    bytes("{ } :", &b, sizeof b);
    return 0;
}
