/* 뚱뚱한 포인터: 자료를 가리키는 포인터와 표를 가리키는 포인터를 함께 들고 다닌다.
   표가 객체 바깥에 있으니, 고칠 수 없는 타입에도 인터페이스를 붙인다 ---
   여기서는 표준 라이브러리의 struct tm 과 그냥 double.
   객체는 아무것도 치르지 않고, 그것을 가리키는 포인터마다 두 배로 넓어진다. */
#include <stdio.h>
#include <time.h>

struct printable_ops { void (*print)(const void *self); const char *type; };
struct printable { const void *self; const struct printable_ops *ops; };   /* 포인터 둘 */

static void print_tm(const void *self)
{
    const struct tm *t = self;
    printf("%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}
static void print_double(const void *self) { printf("%.3f", *(const double *)self); }

/* 견주려고 둔 「vptr」 길: 표 포인터가 객체 안에 있다 */
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
