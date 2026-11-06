/* 컴파일러가 표를 알 때와 모를 때 --- 같은 shape_area 를 다섯 자리에서 부른다.
   run.sh 가 함수마다 간접 호출이 남았는지 기계어로 본다. */
#include <stdlib.h>

struct shape;
struct shape_ops { double (*area)(const struct shape *); };
struct shape { const struct shape_ops *ops; double r; };

static double circle_area(const struct shape *s) { return 3.0 * s->r * s->r; }
static inline double shape_area(const struct shape *s) { return s->ops->area(s); }

static const struct shape_ops circle_ops = { circle_area };  /* static const 표 */
static struct shape_ops quiet_ops = { circle_area };   /* const 없음, 고치는 곳 없음 */
struct shape_ops open_ops = { circle_area };           /* 바깥에서 보이는 표 */
const struct shape_ops *built_ops;                     /* 실행 때 지은 표 */

double known_object(double r)          /* 객체도 표도 여기서 보인다 */
{
    struct shape s = { &circle_ops, r };
    return shape_area(&s);
}
double never_written(double r)         /* const 는 없어도 이 파일에서 쓰는 곳은 없다 */
{
    struct shape s = { &quiet_ops, r };
    return shape_area(&s);
}
double visible_outside(double r)       /* 다른 파일이 고칠 수 있다 */
{
    struct shape s = { &open_ops, r };
    return shape_area(&s);
}
double built_at_run_time(double r)     /* 표가 실행 때 정해진다 */
{
    struct shape s = { built_ops, r };
    return shape_area(&s);
}
/* static const 표 하나뿐이나 객체가 밖에서 온다 */
double from_caller(const struct shape *s)
{
    return shape_area(s);
}

void build(void)
{
    struct shape_ops *t = malloc(sizeof *t);
    if (t) { *t = circle_ops; built_ops = t; }
}
