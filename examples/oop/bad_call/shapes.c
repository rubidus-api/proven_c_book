/* 손으로 만든 「메서드 표」를 아무렇게나 채웠다: circle_area 는 원을 받는데,
   표의 칸은 도형을 받는 함수를 기대한다. 캐스트가 컴파일러를 조용하게 만들었고,
   그것으로 부르는 것은 정의되지 않은 동작이다(C23 6.3.2.3p8). */
#include <stdio.h>

struct shape;
struct shape_ops { double (*area)(const struct shape *); };
struct shape { const struct shape_ops *ops; };
struct circle { struct shape base; double r; };

static double circle_area(const struct circle *c) { return 3.0 * c->r * c->r; }

static const struct shape_ops circle_ops = {
    /* 캐스트가 어긋남을 감춘다 */
    .area = (double (*)(const struct shape *))circle_area,
};

int main(void)
{
    struct circle c = { .base.ops = &circle_ops, .r = 2.0 };
    struct shape *s = &c.base;
    /* 이 기계에서는 「된다」 --- 그래도 틀렸다 */
    printf("area = %g\n", s->ops->area(s));
    return 0;
}
