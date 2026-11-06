/* Inheritance by tables: the derived table holds the base table as its first member.
   - square overrides one slot (area) and inherits describe from shape.
   - circle overrides describe, calls the base version first (an "up-call"),
     and adds a slot of its own (radius) that only circle code can reach. */
#include <stdio.h>

struct shape;
struct shape_ops {
    double (*area)(const struct shape *);
    void   (*describe)(const struct shape *);
};
struct shape { const struct shape_ops *ops; const char *name; };

static void shape_describe(const struct shape *s)          /* the base behaviour */
{
    printf("%-7s area %6.2f", s->name, s->ops->area(s));
}

/* square: a plain derived kind --- one slot replaced, the rest copied from the base */
struct square { struct shape base; double side; };
static double square_area(const struct shape *s)
{
    const struct square *q = (const struct square *)s;      /* base is the first member */
    return q->side * q->side;
}
static const struct shape_ops square_ops = {
    .area = square_area, .describe = shape_describe,
};

/* circle: a derived table that embeds the base table and adds a slot */
struct circle;
struct circle_ops {
    struct shape_ops base;                                  /* must stay first */
    double (*radius)(const struct circle *);
};
struct circle { struct shape base; double r; };

static double circle_area(const struct shape *s)
{
    const struct circle *c = (const struct circle *)s;
    return 3.0 * c->r * c->r;
}
static double circle_radius(const struct circle *c) { return c->r; }
static void circle_describe(const struct shape *s)
{
    shape_describe(s);                                      /* up-call: base first */
    const struct circle_ops *ops = (const struct circle_ops *)s->ops;
    printf("  radius %.1f", ops->radius((const struct circle *)s));
}
static const struct circle_ops circle_ops = {
    .base = { .area = circle_area, .describe = circle_describe },
    .radius = circle_radius,
};

int main(void)
{
    struct square sq = { { &square_ops, "square" }, 2 };
    struct circle ci = { { &circle_ops.base, "circle" }, 1.5 };
    const struct shape *all[] = { &sq.base, &ci.base };

    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) {
        all[i]->ops->describe(all[i]);   /* the caller sees only shape */
        putchar('\n');
    }
    printf("circle's table starts with the shape table: %s\n",
           (const void *)&circle_ops == (const void *)&circle_ops.base ? "yes" : "no");
    return 0;
}
