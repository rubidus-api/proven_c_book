/* 표로 잇는 상속: 파생 표가 기반 표를 첫 멤버로 품는다.
   - square 는 칸 하나(area)를 덮어쓰고 describe 는 shape 의 것을 물려받는다.
   - circle 은 describe 를 덮어쓰되 기반의 것을 먼저 부르고(상위 호출),
     circle 코드만 닿는 자기 칸(radius)을 더한다. */
#include <stdio.h>

struct shape;
struct shape_ops {
    double (*area)(const struct shape *);
    void   (*describe)(const struct shape *);
};
struct shape { const struct shape_ops *ops; const char *name; };

static void shape_describe(const struct shape *s)          /* 기반의 행동 */
{
    printf("%-7s area %6.2f", s->name, s->ops->area(s));
}

/* square: 평범한 파생 --- 칸 하나만 바꾸고 나머지는 기반의 것을 적는다 */
struct square { struct shape base; double side; };
static double square_area(const struct shape *s)
{
    const struct square *q = (const struct square *)s;      /* base 가 첫 멤버다 */
    return q->side * q->side;
}
static const struct shape_ops square_ops = {
    .area = square_area, .describe = shape_describe,
};

/* circle: 기반 표를 품고 칸 하나를 더한 파생 표 */
struct circle;
struct circle_ops {
    struct shape_ops base;                                  /* 반드시 첫 자리 */
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
    shape_describe(s);                                      /* 상위 호출: 기반 먼저 */
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
        all[i]->ops->describe(all[i]);   /* 부르는 쪽은 shape 만 본다 */
        putchar('\n');
    }
    printf("circle's table starts with the shape table: %s\n",
           (const void *)&circle_ops == (const void *)&circle_ops.base ? "yes" : "no");
    return 0;
}
