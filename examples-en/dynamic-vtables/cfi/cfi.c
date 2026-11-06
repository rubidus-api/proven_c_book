/* A table slot holds a function of the wrong type. In C that is undefined behaviour
   (6.3.2.3p8). An ordinary build just calls it; a Clang CFI build stops first. */
#include <stdio.h>

struct shape;
struct shape_ops { double (*area)(const struct shape *); };
struct shape { const struct shape_ops *ops; double r; };

static double circle_area(const struct shape *s) { return 3.0 * s->r * s->r; }
static long   count_lines(const char *text)          /* a function of another type */
{
    long n = 0;
    for (; text && *text; text++) n += *text == '\n';
    return n;
}

/* the cast through void (*)(void) avoids -Wcast-function-type --- it imitates
   the mistake no compiler catches */
static struct shape_ops ops = { circle_area };

int main(void)
{
    struct shape s = { &ops, 2.0 };
    printf("right slot: %.1f\n", s.ops->area(&s));
    fflush(stdout);
    ops.area = (double (*)(const struct shape *))(void (*)(void))count_lines;
    printf("wrong slot: ");
    fflush(stdout);
    double a = s.ops->area(&s);     /* what comes back is garbage --- not printed */
    (void)a;
    puts("the call went through --- nothing stopped it");
    return 0;
}
