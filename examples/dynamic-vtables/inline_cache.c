/* 인라인 캐시와 다형 인라인 캐시 --- 호출 자리마다 캐시를 둔다.
   Smalltalk 와 JIT 런타임은 호출하는 기계어 자체를 고쳐 쓴다. C 는 제 코드를
   고쳐 쓸 수 없으므로, 호출 옆에 static 변수를 두고 그 호출만 그것을 쓴다.
   - 한 칸(IC): 「지난번 클래스 → 함수」 하나.
   - 네 칸(PIC): 그런 칸 넷. 놓치면 칸을 차례로 바꾼다.
   - 메가모픽: 다 찬 PIC 가 놓치면 그 자리는 비교를 그만두고
     클래스별 캐시로 간다.
   칸마다 세대 번호를 적는다. 72장의 lookup.c 와 같은 까닭이다 ---
   실행 때 바뀐 메서드가 캐시에 남으면 안 된다. */
#include <stdio.h>

struct object;
typedef double (*imp)(const struct object *self);

struct method { unsigned sel; imp fn; };
struct entry { const struct class *cls; imp fn; unsigned epoch; };
struct class {
    const struct class *parent;
    struct method methods[2];
    unsigned n_methods;
    double factor;
    struct entry cache;                  /* 클래스별 캐시, 여기서는 셀렉터 하나 */
};
struct object { struct class *isa; double size; };

enum { SEL_AREA = 1 };

static unsigned epoch = 1;               /* 메서드가 바뀔 때마다 오른다 */
static unsigned slow_lookups;

static imp lookup_slow(const struct class *c, unsigned sel)
{
    slow_lookups++;
    for (; c; c = c->parent)
        for (unsigned i = 0; i < c->n_methods; i++)
            if (c->methods[i].sel == sel) return c->methods[i].fn;
    return nullptr;
}
static imp lookup_class_cache(struct class *c, unsigned sel)
{
    if (c->cache.epoch != epoch)
        c->cache = (struct entry){ c, lookup_slow(c, sel), epoch };
    return c->cache.fn;
}

/* ---- 호출 자리 하나의 캐시 ---- */
/* 셀렉터는 적지 않는다: 한 자리에서 보내는 메시지는 늘 같다. */
struct site {
    struct entry e[4];
    unsigned ways, next;                 /* 쓰는 칸 수(1 또는 4), 다음에 바꿀 칸 */
    bool megamorphic;
    unsigned sends, misses;
};

static double send_at(struct site *s, struct object *o, unsigned sel)
{
    s->sends++;
    if (s->megamorphic)                  /* 비교를 그만둠: 클래스별 캐시 */
        return lookup_class_cache(o->isa, sel)(o);
    for (unsigned i = 0; i < s->ways; i++)
        if (s->e[i].cls == o->isa && s->e[i].epoch == epoch)
            return s->e[i].fn(o);        /* 맞음: 칸마다 비교 하나, 찾기 없음 */
    s->misses++;
    if (s->ways > 1 && s->e[s->next].cls && s->e[s->next].epoch == epoch) {
        s->megamorphic = true;           /* 다 찼는데 놓침: 비교를 접는다 */
        return lookup_class_cache(o->isa, sel)(o);
    }
    imp fn = lookup_slow(o->isa, sel);
    s->e[s->next] = (struct entry){ o->isa, fn, epoch };
    s->next = (s->next + 1) % s->ways;
    return fn(o);
}

static const char *state(const struct site *s)
{
    if (s->megamorphic) return "megamorphic";
    unsigned used = 0;
    for (unsigned i = 0; i < s->ways; i++) used += s->e[i].epoch == epoch;
    return used == 0 ? "empty" : used == 1 ? "monomorphic" : "polymorphic";
}
static void report(const char *label, struct site *s, double sum)
{
    printf("  %-12s sum %5g, %2u sends, %2u misses, %-11s (slow lookups %u)\n",
           label, sum, s->sends, s->misses, state(s), slow_lookups);
    s->sends = s->misses = 0;
    slow_lookups = 0;
}

/* 호출 자리 둘. 블록 안의 static 이 그 아래 호출 하나의 캐시다. */
static void total_area_ic(struct object *xs, size_t n)
{
    static struct site here = { .ways = 1 };   /* 이 호출 자리만의 캐시 */
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += send_at(&here, &xs[i], SEL_AREA);
    report("1-entry IC", &here, sum);
}
static void total_area_pic(struct object *xs, size_t n)
{
    static struct site here = { .ways = 4 };   /* 이 자리의 것 */
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += send_at(&here, &xs[i], SEL_AREA);
    report("4-entry PIC", &here, sum);
}

/* ---- 클래스들: 넓이는 모두 계수 × 크기 × 크기 ---- */
static double scaled_area(const struct object *self)
{ return self->isa->factor * self->size * self->size; }
static double circle_area(const struct object *self)
{ return 3 * self->size * self->size; }
static double circle_area_2(const struct object *self)      /* 실행 때 바꿔 넣는다 */
{ return 3.14 * self->size * self->size; }

static struct class shape = {
    .methods = { { SEL_AREA, scaled_area } }, .n_methods = 1, .factor = 1 };
static struct class circle = {
    .parent = &shape, .methods = { { SEL_AREA, circle_area } }, .n_methods = 1 };
#define KIND(var, f) static struct class var = { .parent = &shape, .factor = f }
KIND(square, 1); KIND(triangle, 0.5); KIND(hexagon, 2.5);
KIND(kite, 0.5); KIND(star, 4);

static void scene(const char *title, struct object *xs, size_t n)
{
    puts(title);
    total_area_ic(xs, n);
    total_area_pic(xs, n);
}

int main(void)
{
    struct object same[8], two[8], six[12];
    struct class *six_kinds[] = { &circle, &square, &triangle, &hexagon, &kite, &star };
    for (size_t i = 0; i < 8; i++) {
        same[i] = (struct object){ &circle, 1 };
        two[i] = (struct object){ i % 2 ? &square : &circle, 1 };
    }
    for (size_t i = 0; i < 12; i++) six[i] = (struct object){ six_kinds[i % 6], 1 };

    scene("one class only (8 circles):", same, 8);
    scene("two classes taking turns (circle, square, ...):", two, 8);
    scene("two classes again, after the first round:", two, 8);
    scene("six classes taking turns:", six, 12);

    puts("change Circle's area at run time (generation rises), then two classes:");
    circle.methods[0].fn = circle_area_2;
    epoch++;
    total_area_ic(two, 8);
    total_area_pic(two, 8);
    return 0;
}
