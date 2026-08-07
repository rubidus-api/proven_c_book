#include <stdio.h>
#include <string.h>

/* C 에서 손으로 만드는 객체지향: 가상 함수 표(vtable)를 자료와 분리한다.
   GTK 의 GObject 가 쓰는 방식과 같은 뼈대다. */

struct shape;                       /* 앞선 선언 */

/* ① 가상 함수 표 — 타입마다 하나만 존재한다 (인스턴스마다가 아니다) */
struct shape_vtable {
    const char *name;
    double (*area)(const struct shape *self);
    void   (*describe)(const struct shape *self);
};

/* ② 기반 "클래스": 모든 객체의 첫 멤버가 표를 가리킨다 */
struct shape {
    const struct shape_vtable *vt;
};

/* ③ 파생 "클래스": 기반을 첫 멤버로 두면 포인터를 서로 변환할 수 있다 */
struct circle { struct shape base; double r; };
struct rect   { struct shape base; double w, h; };

static double circle_area(const struct shape *s)
{
    const struct circle *c = (const struct circle *)s;   /* 첫 멤버라 주소가 같다 */
    return 3.14159265358979 * c->r * c->r;
}
static double rect_area(const struct shape *s)
{
    const struct rect *r = (const struct rect *)s;
    return r->w * r->h;
}
static void generic_describe(const struct shape *s)
{
    printf("  %-6s 넓이 %.2f\n", s->vt->name, s->vt->area(s));
}

/* ④ 표는 상수이고 타입마다 하나 — 인스턴스는 포인터만 갖는다 */
static const struct shape_vtable circle_vt = { "원",   circle_area, generic_describe };
static const struct shape_vtable rect_vt   = { "사각", rect_area,   generic_describe };

static struct circle make_circle(double r) { return (struct circle){ { &circle_vt }, r }; }
static struct rect   make_rect(double w, double h) { return (struct rect){ { &rect_vt }, w, h }; }

int main(void)
{
    struct circle c = make_circle(2.0);
    struct rect   r = make_rect(3.0, 4.0);

    /* ⑤ 같은 코드가 서로 다른 타입을 다룬다 — 다형성 */
    const struct shape *objs[] = { &c.base, &r.base };
    for (size_t i = 0; i < sizeof objs / sizeof objs[0]; i++)
        objs[i]->vt->describe(objs[i]);

    /* ⑥ 비용을 눈으로 본다 */
    printf("객체 크기: circle=%zu, rect=%zu (표 포인터 %zu 바이트 포함)\n",
           sizeof(struct circle), sizeof(struct rect), sizeof(void *));
    printf("표 크기  : %zu (타입마다 하나만 존재)\n", sizeof(struct shape_vtable));
    return 0;
}
