/* 도형 백만 개의 넓이 합을 세 가지로 구한다.
   (1) 객체: 도형마다 따로 할당하고, 메서드 표를 거쳐 부른다
   (2) 태그 달린 구조체를 한 배열에, 태그로 switch
   (3) 도형의 종류마다 배열 하나, 배열마다 평범한 반복문
   ★ 이 예제는 시간을 잰다. 배수만 인쇄한다. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N      (1u << 20)
#define ROUNDS 5

/* (1) 객체의 길 */
struct shape;
struct shape_ops { double (*area)(const struct shape *); };
struct shape { const struct shape_ops *ops; };
struct circle { struct shape base; double r; };
struct rect   { struct shape base; double w, h; };
static double circle_area(const struct shape *s)
{ const struct circle *c = (const struct circle *)s; return 3.14159 * c->r * c->r; }
static double rect_area(const struct shape *s)
{ const struct rect *r = (const struct rect *)s; return r->w * r->h; }
static const struct shape_ops circle_ops = { circle_area }, rect_ops = { rect_area };

/* (2) 태그의 길 */
enum kind { CIRCLE, RECT };
struct tagged { enum kind kind; double a, b; };

/* (3) 종류별 배열의 길 */
static double *radius, *width, *height;
static uint32_t n_circles, n_rects;

static struct shape **objs;
static struct tagged *tags;
static volatile double sink;

static double now(void) { return (double)clock() / CLOCKS_PER_SEC; }
static double by_table(void)
{
    double t = now(), s = 0;
    for (uint32_t i = 0; i < N; i++) s += objs[i]->ops->area(objs[i]);
    sink = s; return now() - t;
}
static double by_switch(void)
{
    double t = now(), s = 0;
    for (uint32_t i = 0; i < N; i++)
        switch (tags[i].kind) {
        case CIRCLE: s += 3.14159 * tags[i].a * tags[i].a; break;
        case RECT:   s += tags[i].a * tags[i].b; break;
        }
    sink = s; return now() - t;
}
static double by_kind(void)
{
    double t = now(), s = 0;
    for (uint32_t i = 0; i < n_circles; i++) s += 3.14159 * radius[i] * radius[i];
    for (uint32_t i = 0; i < n_rects; i++)   s += width[i] * height[i];
    sink = s; return now() - t;
}

static int cmp(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static double median(double (*job)(void))
{
    double r[ROUNDS];
    job();
    for (int i = 0; i < ROUNDS; i++) r[i] = job();
    qsort(r, ROUNDS, sizeof *r, cmp);
    return r[ROUNDS / 2];
}

int main(void)
{
    objs = malloc(N * sizeof *objs); tags = malloc(N * sizeof *tags);
    radius = malloc(N * sizeof *radius);
    width = malloc(N * sizeof *width);
    height = malloc(N * sizeof *height);
    if (!objs || !tags || !radius || !width || !height)
        { puts("out of memory"); return 1; }

    uint64_t rng = 0x9E3779B97F4A7C15u;
    for (uint32_t i = 0; i < N; i++) {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        double x = (double)(i % 100) / 10.0;
        if (rng >> 63) {                     /* 종류가 무작위 순서로 온다 */
            struct circle *c = malloc(sizeof *c);
            if (!c) { puts("out of memory"); return 1; }
            *c = (struct circle){ { &circle_ops }, x };
            objs[i] = &c->base;
            tags[i] = (struct tagged){ CIRCLE, x, 0 };
            radius[n_circles++] = x;
        } else {
            struct rect *r = malloc(sizeof *r);
            if (!r) { puts("out of memory"); return 1; }
            *r = (struct rect){ { &rect_ops }, x, 2 };
            objs[i] = &r->base;
            tags[i] = (struct tagged){ RECT, x, 2 };
            width[n_rects] = x; height[n_rects++] = 2;
        }
    }

    double t = median(by_table), s = median(by_switch), k = median(by_kind);
    printf("%u shapes, circles and rectangles in random order\n", N);
    if (k > 0) {
        printf("  method table : %5.1f times as long as per-kind arrays\n", t / k);
        printf("  tag + switch : %5.1f times as long as per-kind arrays\n", s / k);
    } else puts("  the clock is too coarse on this machine to tell them apart");

    for (uint32_t i = 0; i < N; i++) free(objs[i]);
    free(objs); free(tags); free(radius); free(width); free(height);
    return 0;
}
