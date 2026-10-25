/* 종류가 닫혀 있을 때 메서드 표 대신 쓰는 길: 태그, 공용체, 그리고 switch.
   default 를 일부러 두지 않았다 --- 그래야 새 종류의 도형이 더해졌는데 이 함수가
   그것을 모를 때 컴파일러가 알려 준다. */
#include <stdio.h>

enum shape_kind {
    SHAPE_CIRCLE,
    SHAPE_RECT,
#ifdef ADD_TRIANGLE
    SHAPE_TRIANGLE,              /* 나중에 더한 새 종류 */
#endif
};

struct shape {
    enum shape_kind kind;
    union {
        struct { double r; } circle;
        struct { double w, h; } rect;
#ifdef ADD_TRIANGLE
        struct { double base, height; } triangle;
#endif
    };
};

static double area(const struct shape *s)
{
    switch (s->kind) {
    case SHAPE_CIRCLE: return 3.0 * s->circle.r * s->circle.r;
    case SHAPE_RECT:   return s->rect.w * s->rect.h;
    }
    return 0.0;                  /* 올바른 종류라면 닿지 않는다 */
}

int main(void)
{
    struct shape a[] = {
        { .kind = SHAPE_CIRCLE, .circle = { 1.0 } },
        { .kind = SHAPE_RECT,   .rect = { 2.0, 3.0 } },
    };
    double total = 0;
    for (size_t i = 0; i < sizeof a / sizeof a[0]; i++) total += area(&a[i]);
    printf("total area %g\n", total);
    return 0;
}
