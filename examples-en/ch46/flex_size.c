/* Sizing a flexible array member: when do `sizeof` and `offsetof` agree, and when not? */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* (1) the shape where they agree: the members leave no tail padding behind */
struct plain {
    uint32_t len;
    char     data[];
};

/* (2) the shape where they differ: the struct's alignment (8) does not divide
      the last member's offset (12), so sizeof rounds up to 16 and those four
      bytes are the tail padding. */
struct rec {
    double   stamp;
    uint32_t count;
    uint16_t data[];
};

/* the size each rule asks for */
static size_t by_offsetof(size_t n) { return offsetof(struct rec, data) + n * sizeof(uint16_t); }
static size_t by_sizeof(size_t n)   { return sizeof(struct rec)         + n * sizeof(uint16_t); }

int main(void)
{
    puts("(1) when the two rules agree");
    printf("  struct plain: offsetof(data) = %zu, sizeof = %zu  -> same\n",
           offsetof(struct plain, data), sizeof(struct plain));

    puts("\n(2) when they do not");
    printf("  struct rec:   offsetof(data) = %zu, sizeof = %zu, alignof = %zu\n",
           offsetof(struct rec, data), sizeof(struct rec), alignof(struct rec));
    printf("  tail padding  = sizeof - offsetof(data) = %zu bytes\n",
           sizeof(struct rec) - offsetof(struct rec, data));

    puts("\n(3) bytes reserved for n elements");
    puts("   n | offsetof rule | sizeof rule | wasted");
    for (size_t n = 0; n <= 4; n++)
        printf("  %2zu | %13zu | %11zu | %6zu\n",
               n, by_offsetof(n), by_sizeof(n), by_sizeof(n) - by_offsetof(n));

    /* So far this is only "a little more than needed". The standard guarantees
       sizeof >= offsetof, so the sizeof rule is never *too small*. The difference
       changes the answer when the size question is asked *backwards*. */

    puts("\n(4) the same arithmetic, run backwards: how many elements fit?");
    size_t buf = by_offsetof(3);          /* exactly the size that holds three */
    printf("  a buffer of %zu bytes holds exactly 3 elements\n", buf);
    printf("  offsetof rule says (%zu - %zu) / %zu = %zu   <- right\n",
           buf, offsetof(struct rec, data), sizeof(uint16_t),
           (buf - offsetof(struct rec, data)) / sizeof(uint16_t));
    printf("  sizeof   rule says (%zu - %zu) / %zu = %zu   <- two elements lost\n",
           buf, sizeof(struct rec), sizeof(uint16_t),
           (buf - sizeof(struct rec)) / sizeof(uint16_t));

    puts("\n(5) is an empty record well formed?");
    size_t empty = by_offsetof(0);
    printf("  an empty record on the wire is %zu bytes\n", empty);
    printf("  \"len >= offsetof(data)\" -> %s   <- right\n",
           empty >= offsetof(struct rec, data) ? "accepted" : "REJECTED");
    printf("  \"len >= sizeof(struct)\" -> %s  <- a valid record thrown away\n",
           empty >= sizeof(struct rec) ? "accepted" : "REJECTED");

    puts("\n(6) the price of the exact fit");
    printf("  offsetof rule for n=1 reserves %zu bytes, but sizeof(struct rec) is %zu\n",
           by_offsetof(1), sizeof(struct rec));
    puts("  so the object is smaller than its own type: *a = *b, memcpy(a, b, sizeof *a),");
    puts("  or passing it by value would run past the end of the allocation.");
    puts("  a struct with a flexible array member is copied field by field, never whole.");

    /* allocate for real, and see where the last element ends */
    size_t n = 3;
    struct rec *r = malloc(by_offsetof(n));
    if (!r) { perror("malloc"); return 1; }
    r->stamp = 1.5;
    r->count = (uint32_t)n;
    for (size_t i = 0; i < n; i++) r->data[i] = (uint16_t)(10 * (i + 1));

    printf("\n  allocated %zu bytes; data[%zu] ends at offset %zu\n",
           by_offsetof(n), n - 1,
           offsetof(struct rec, data) + n * sizeof(uint16_t));
    printf("  data = %u %u %u\n", r->data[0], r->data[1], r->data[2]);
    free(r);
    return 0;
}
