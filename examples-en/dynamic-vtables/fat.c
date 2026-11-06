/* Fat pointers: a pointer to the data and a pointer to the table, carried together.
   The table lives outside the object, so an interface can be attached to types we
   cannot change --- here the standard library's struct tm and a plain double.
   The object pays nothing; every pointer to it is twice as wide. */
#include <stdio.h>
#include <time.h>

struct printable_ops { void (*print)(const void *self); const char *type; };
struct printable { const void *self; const struct printable_ops *ops; };   /* fat */

static void print_tm(const void *self)
{
    const struct tm *t = self;
    printf("%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}
static void print_double(const void *self) { printf("%.3f", *(const double *)self); }

/* the "vptr" way, for comparison: the table pointer is inside the object */
struct point_ops;
struct point { const struct point_ops *ops; double x, y; };
struct point_ops { void (*print)(const struct point *); };
static void print_point(const struct point *p) { printf("(%g, %g)", p->x, p->y); }
static const struct point_ops point_ops = { print_point };
static void print_point_any(const void *self) { print_point(self); }

static const struct printable_ops tm_printable     = { print_tm, "struct tm" };
static const struct printable_ops double_printable = { print_double, "double" };
static const struct printable_ops point_printable  = { print_point_any, "struct point" };

static void print_all(const struct printable *xs, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        printf("  %-13s ", xs[i].ops->type);
        xs[i].ops->print(xs[i].self);
        putchar('\n');
    }
}

int main(void)
{
    struct tm day = { .tm_year = 2026 - 1900, .tm_mon = 10, .tm_mday = 6 };
    double ratio = 1.618;
    struct point p = { &point_ops, 3, 4 };

    struct printable xs[] = {
        { &day, &tm_printable }, { &ratio, &double_printable }, { &p, &point_printable },
    };
    puts("one interface, three types --- two of them never heard of it:");
    print_all(xs, sizeof xs / sizeof xs[0]);

    printf("sizeof(struct tm) %zu, unchanged; sizeof(double) %zu, unchanged\n",
           sizeof day, sizeof ratio);
    printf("sizeof(struct point) %zu --- it carries its own table pointer\n", sizeof p);
    printf("a plain pointer %zu bytes, a fat pointer %zu bytes\n",
           sizeof(void *), sizeof(struct printable));
    return 0;
}
