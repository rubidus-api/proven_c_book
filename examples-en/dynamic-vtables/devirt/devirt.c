/* When the compiler knows the table, and when it does not --- the same shape_area,
   called from five places. run.sh checks each function's machine code for an
   indirect call. */
#include <stdlib.h>

struct shape;
struct shape_ops { double (*area)(const struct shape *); };
struct shape { const struct shape_ops *ops; double r; };

static double circle_area(const struct shape *s) { return 3.0 * s->r * s->r; }
static inline double shape_area(const struct shape *s) { return s->ops->area(s); }

static const struct shape_ops circle_ops = { circle_area };  /* a static const table */
static struct shape_ops quiet_ops = { circle_area };   /* no const, nobody changes it */
struct shape_ops open_ops = { circle_area };           /* visible to other files */
const struct shape_ops *built_ops;                     /* built at run time */

double known_object(double r)          /* object and table both visible here */
{
    struct shape s = { &circle_ops, r };
    return shape_area(&s);
}
double never_written(double r)         /* no const, yet nothing in this file writes it */
{
    struct shape s = { &quiet_ops, r };
    return shape_area(&s);
}
double visible_outside(double r)       /* another file may change it */
{
    struct shape s = { &open_ops, r };
    return shape_area(&s);
}
double built_at_run_time(double r)     /* the table is decided at run time */
{
    struct shape s = { built_ops, r };
    return shape_area(&s);
}
/* one static const table, but the object comes from outside */
double from_caller(const struct shape *s)
{
    return shape_area(s);
}

void build(void)
{
    struct shape_ops *t = malloc(sizeof *t);
    if (t) { *t = circle_ops; built_ops = t; }
}
