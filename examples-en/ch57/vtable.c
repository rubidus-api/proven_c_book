#include <stdio.h>
#include <string.h>

/* Object orientation built by hand in C: the table of virtual functions
   (a vtable) is kept apart from the data. The same skeleton GTK's GObject uses. */

struct shape;                       /* a forward declaration */

/* (1) the vtable — one per type, not one per instance */
struct shape_vtable {
    const char *name;
    double (*area)(const struct shape *self);
    void   (*describe)(const struct shape *self);
};

/* (2) the base "class": the first member of every object points at the table */
struct shape {
    const struct shape_vtable *vt;
};

/* (3) the derived "class": put the base first and the pointers convert to each other */
struct circle { struct shape base; double r; };
struct rect   { struct shape base; double w, h; };

static double circle_area(const struct shape *s)
{
    const struct circle *c = (const struct circle *)s;   /* being the first member, the address is the same */
    return 3.14159265358979 * c->r * c->r;
}
static double rect_area(const struct shape *s)
{
    const struct rect *r = (const struct rect *)s;
    return r->w * r->h;
}
static void generic_describe(const struct shape *s)
{
    printf("  %-9s area %.2f\n", s->vt->name, s->vt->area(s));
}

/* (4) the table is constant and one per type — an instance holds only a pointer */
static const struct shape_vtable circle_vt = { "circle",    circle_area, generic_describe };
static const struct shape_vtable rect_vt   = { "rectangle", rect_area,   generic_describe };

static struct circle make_circle(double r) { return (struct circle){ { &circle_vt }, r }; }
static struct rect   make_rect(double w, double h) { return (struct rect){ { &rect_vt }, w, h }; }

int main(void)
{
    struct circle c = make_circle(2.0);
    struct rect   r = make_rect(3.0, 4.0);

    /* (5) the same code handles different types — polymorphism */
    const struct shape *objs[] = { &c.base, &r.base };
    for (size_t i = 0; i < sizeof objs / sizeof objs[0]; i++)
        objs[i]->vt->describe(objs[i]);

    /* (6) the cost, seen with the eyes */
    printf("object sizes: circle=%zu, rect=%zu (the %zu-byte table pointer included)\n",
           sizeof(struct circle), sizeof(struct rect), sizeof(void *));
    printf("table size  : %zu (only one exists per type)\n", sizeof(struct shape_vtable));
    return 0;
}
