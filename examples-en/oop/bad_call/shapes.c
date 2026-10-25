/* A hand-made "method table" filled in carelessly: circle_area takes a circle,
   but the table's slot expects a function taking a shape. The cast makes the compiler
   quiet; calling through it is undefined behaviour (C23 6.3.2.3p8). */
#include <stdio.h>

struct shape;
struct shape_ops { double (*area)(const struct shape *); };
struct shape { const struct shape_ops *ops; };
struct circle { struct shape base; double r; };

static double circle_area(const struct circle *c) { return 3.0 * c->r * c->r; }

static const struct shape_ops circle_ops = {
    /* the cast hides the mismatch */
    .area = (double (*)(const struct shape *))circle_area,
};

int main(void)
{
    struct circle c = { .base.ops = &circle_ops, .r = 2.0 };
    struct shape *s = &c.base;
    /* "works" on this machine --- and is still wrong */
    printf("area = %g\n", s->ops->area(s));
    return 0;
}
