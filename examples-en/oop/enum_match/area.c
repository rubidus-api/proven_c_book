/* The closed-set alternative to a method table: a tag, a union, and a switch.
   No default label, on purpose: then the compiler can tell us when a new kind
   of shape has been added and this function was not taught about it. */
#include <stdio.h>

enum shape_kind {
    SHAPE_CIRCLE,
    SHAPE_RECT,
#ifdef ADD_TRIANGLE
    SHAPE_TRIANGLE,              /* the new kind, added later */
#endif
};

struct shape {
    enum shape_kind kind;
    union {
        struct { double r; } circle;
        struct { double w, h; } rect;
#ifdef ADD_TRIANGLE
        struct { double base, height; } triangle;
#endif
    };
};

static double area(const struct shape *s)
{
    switch (s->kind) {
    case SHAPE_CIRCLE: return 3.0 * s->circle.r * s->circle.r;
    case SHAPE_RECT:   return s->rect.w * s->rect.h;
    }
    return 0.0;                  /* not reached for a valid kind */
}

int main(void)
{
    struct shape a[] = {
        { .kind = SHAPE_CIRCLE, .circle = { 1.0 } },
        { .kind = SHAPE_RECT,   .rect = { 2.0, 3.0 } },
    };
    double total = 0;
    for (size_t i = 0; i < sizeof a / sizeof a[0]; i++) total += area(&a[i]);
    printf("total area %g\n", total);
    return 0;
}
