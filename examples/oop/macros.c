/* 객체 체계를 돕는 매크로와, 해치는 매크로 하나.
   (1) 타입이 틀린 포인터를 거절하는 container_of
   (2) GObject 의 GTK_WIDGET(x) 같은 결의 검사하는 하향 캐스트
   (3) 인자를 두 번 평가하는 「표를 거쳐 부르기」 매크로 --- 그리고 그 고침 */
#include <stddef.h>
#include <stdio.h>

/* (1) sizeof 안의 비교는 평가되지 않지만 타입 검사는 받는다:
   다른 타입의 포인터를 비교하는 것은 제약 위반이라 컴파일러가 진단해야 하고,
   -Werror 를 켜면 틀린 `ptr` 이 빌드를 멈춘다. */
#define container_of(ptr, type, member)                                   \
    ((void)sizeof((ptr) == &((type *)nullptr)->member),                   \
     (type *)((char *)(ptr) - offsetof(type, member)))

enum kind { KIND_CIRCLE = 0xC1, KIND_RECT = 0x4E };
struct shape;
struct shape_ops { enum kind kind; double (*area)(const struct shape *); };
struct shape  { const struct shape_ops *ops; };
/* base 가 첫 멤버가 아니어도 container_of 는 된다 */
struct circle { double r; struct shape base; };

static double circle_area(const struct shape *s)
{
    const struct circle *c = container_of(s, const struct circle, base);
    return 3.0 * c->r * c->r;
}
static const struct shape_ops circle_ops = { KIND_CIRCLE, circle_area };
static const struct shape_ops rect_ops   = { KIND_RECT, nullptr };

/* (2) 캐스트를 믿기 전에 종류를 본다. 틀린 캐스트는 쓰레기가 아니라 nullptr 가 된다.
   (`s` 를 세 번 적으므로 이것에도 평범한 변수만 넘긴다 --- s++ 는 안 된다.) */
#define AS_CIRCLE(s) ((s) && (s)->ops->kind == KIND_CIRCLE                  \
                      ? container_of((s), struct circle, base) : (struct circle *)nullptr)

/* (3) 무해해 보이지만… */
#define SHAPE_AREA(s) ((s)->ops->area(s))
/* …함수로 쓰는 편이 안전하다: 인자를 정확히 한 번 평가하고, 타입도 검사한다 */
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
    /* container_of(&c.r, struct circle, base) 는 안 된다: &c.r 은 double * 다. */

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
