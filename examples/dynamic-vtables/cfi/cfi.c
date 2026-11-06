/* 표의 칸에 타입이 틀린 함수가 들어갔다. C 로는 정의되지 않은 동작이다(6.3.2.3p8).
   보통 빌드는 그냥 부르고, Clang 의 CFI 빌드는 부르기 전에 멈춘다. */
#include <stdio.h>

struct shape;
struct shape_ops { double (*area)(const struct shape *); };
struct shape { const struct shape_ops *ops; double r; };

static double circle_area(const struct shape *s) { return 3.0 * s->r * s->r; }
static long   count_lines(const char *text)          /* 타입이 다른 함수 */
{
    long n = 0;
    for (; text && *text; text++) n += *text == '\n';
    return n;
}

/* void (*)(void) 를 거치는 변환은 -Wcast-function-type 을 피한다 ---
   컴파일러가 잡지 못하는 실수를 흉내 낸다 */
static struct shape_ops ops = { circle_area };

int main(void)
{
    struct shape s = { &ops, 2.0 };
    printf("right slot: %.1f\n", s.ops->area(&s));
    fflush(stdout);
    ops.area = (double (*)(const struct shape *))(void (*)(void))count_lines;
    printf("wrong slot: ");
    fflush(stdout);
    double a = s.ops->area(&s);     /* 돌아온 값은 쓰레기다 --- 찍지 않는다 */
    (void)a;
    puts("the call went through --- nothing stopped it");
    return 0;
}
