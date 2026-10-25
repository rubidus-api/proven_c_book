/* Macros that help an object system, and one that hurts.
   (1) container_of that refuses a pointer of the wrong type
   (2) a checked down-cast in the spirit of GObject's GTK_WIDGET(x)
   (3) a call-through-the-table macro that evaluates its argument twice --- and its fix */
#include <stddef.h>
#include <stdio.h>

/* (1) The comparison inside sizeof is never evaluated, but it must type-check:
   comparing pointers to different types is a constraint violation, so the compiler must
   say so --- and with -Werror a wrong `ptr` stops the build. */
#define container_of(ptr, type, member)                                   \
    ((void)sizeof((ptr) == &((type *)nullptr)->member),                   \
     (type *)((char *)(ptr) - offsetof(type, member)))

enum kind { KIND_CIRCLE = 0xC1, KIND_RECT = 0x4E };
struct shape;
struct shape_ops { enum kind kind; double (*area)(const struct shape *); };
struct shape  { const struct shape_ops *ops; };
/* base not first: container_of still works */
struct circle { double r; struct shape base; };

static double circle_area(const struct shape *s)
{
    const struct circle *c = container_of(s, const struct circle, base);
    return 3.0 * c->r * c->r;
}
static const struct shape_ops circle_ops = { KIND_CIRCLE, circle_area };
static const struct shape_ops rect_ops   = { KIND_RECT, nullptr };

/* (2) check the kind before trusting the cast; a wrong cast yields nullptr, not garbage.
   (It names `s` three times, so it too must be given a plain variable, never s++.) */
#define AS_CIRCLE(s) ((s) && (s)->ops->kind == KIND_CIRCLE                  \
                      ? container_of((s), struct circle, base) : (struct circle *)nullptr)

/* (3) looks harmless... */
#define SHAPE_AREA(s) ((s)->ops->area(s))
/* ...is safer as a function: the argument is evaluated once, and it type-checks */
static inline double shape_area(const struct shape *s) { return s->ops->area(s); }

static int evaluations;
static struct shape *next_shape(struct shape *s) { evaluations++; return s; }

int main(void)
{
    struct circle c = { .r = 2.0, .base.ops = &circle_ops };
    struct shape *s = &c.base;
    struct shape rect = { &rect_ops }, *other = &rect;

    printf("container_of: back to the circle, r = %g\n",
           container_of(s, struct circle, base)->r);
    /* container_of(&c.r, struct circle, base) would not compile: &c.r is a double *. */

    struct circle *ok = AS_CIRCLE(s), *no = AS_CIRCLE(other);
    printf("AS_CIRCLE(circle)    : %s\n", ok ? "a circle" : "nullptr");
    printf("AS_CIRCLE(rectangle) : %s\n", no ? "a circle" : "nullptr");

    evaluations = 0;
    double a = SHAPE_AREA(next_shape(s));
    printf("SHAPE_AREA(next_shape(s)) = %g, next_shape ran %d times\n", a, evaluations);
    evaluations = 0;
    a = shape_area(next_shape(s));
    printf("shape_area(next_shape(s)) = %g, next_shape ran %d time\n", a, evaluations);
    return 0;
}
