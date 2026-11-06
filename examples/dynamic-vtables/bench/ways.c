/* 함수를 고르는 길들 --- harness.c 가 잰다.
   - 종류마다 함수가 따로 있고(상수가 달라 합쳐지지 않는다), 어느 것도 인라인하지
     못한다: 재는 것은 일이 아니라 *고르기*다.
   - 하나의 예외는 마지막 길이다. 컴파일러가 인라인하게 둔다. */
#include "bench.h"

#define KIND(k, c)                                                              \
    [[gnu::noinline]] static uint64_t f##k(const struct obj *o)                 \
    { return o->x * (c) + (k); }                                                \
    static inline uint64_t g##k(const struct obj *o) { return o->x * (c) + (k); }
KIND(0, 3)  KIND(1, 5)  KIND(2, 7)  KIND(3, 11)
KIND(4, 13) KIND(5, 17) KIND(6, 19) KIND(7, 23)

[[gnu::noinline]] uint64_t other(const struct obj *o) { return o->x; }
const method fs[KMAX] = { f0, f1, f2, f3, f4, f5, f6, f7 };
const struct ops static_ops[KMAX] = {
    { f0, other }, { f1, other }, { f2, other }, { f3, other },
    { f4, other }, { f5, other }, { f6, other }, { f7, other },
};

#define EACH for (uint32_t r = 0; r < REPS; r++) for (uint32_t i = 0; i < N; i++)

uint64_t m_direct(void)          /* 기준: 종류 하나, 직접 호출 */
{ uint64_t s = 0; EACH s += f0(&arena[i]); return s; }

uint64_t m_grouped(void)         /* 종류별로 모아(종류마다 색인 목록) 직접 호출 */
{
    uint64_t s = 0;
    for (uint32_t r = 0; r < REPS; r++)
        for (uint32_t k = 0; k < kinds; k++) {
            const uint32_t *ix = by_kind[k];
            switch (k) {
#define GROUP(j) \
    case j: for (uint32_t i = 0; i < count[j]; i++) s += f##j(&arena[ix[i]]); break;
            GROUP(0) GROUP(1) GROUP(2) GROUP(3) GROUP(4) GROUP(5) GROUP(6) GROUP(7)
            }
        }
    return s;
}

uint64_t m_switch(void)          /* 종류 번호로 switch */
{
    uint64_t s = 0;
    EACH {
        const struct obj *o = &arena[i];
        switch (o->kind) {
        case 0: s += f0(o); break; case 1: s += f1(o); break;
        case 2: s += f2(o); break; case 3: s += f3(o); break;
        case 4: s += f4(o); break; case 5: s += f5(o); break;
        case 6: s += f6(o); break; case 7: s += f7(o); break;
        }
    }
    return s;
}

uint64_t m_binary(void)          /* 종류 번호로 이진 분기, 표 없음 */
{
    uint64_t s = 0;
    EACH {
        const struct obj *o = &arena[i];
        uint32_t k = o->kind;
        if (k < 4) s += k < 2 ? (k < 1 ? f0(o) : f1(o)) : (k < 3 ? f2(o) : f3(o));
        else       s += k < 6 ? (k < 5 ? f4(o) : f5(o)) : (k < 7 ? f6(o) : f7(o));
    }
    return s;
}

uint64_t m_static(void)          /* static const 표 */
{ uint64_t s = 0; EACH s += arena[i].sops->apply(&arena[i]); return s; }

uint64_t m_dynamic(void)         /* 실행 때 지은 표 */
{ uint64_t s = 0; EACH s += arena[i].dops->apply(&arena[i]); return s; }

uint64_t m_fat(void)             /* 뚱뚱한 포인터 */
{ uint64_t s = 0; EACH s += fats[i].ops->apply(fats[i].data); return s; }

/* ---- 셀렉터로: Objective-C 의 길, 캐시가 있을 때와 없을 때 ---- */
/* 목록을 훑고, 없으면 부모의 목록을 */
static method lookup_slow(const struct klass *c, uint32_t sel)
{
    for (; c; c = c->parent)
        for (uint32_t m = 0; m < c->n; m++)
            if (c->methods[m].sel == sel) return c->methods[m].fn;
    return nullptr;
}
/* 클래스별 캐시를 먼저 */
static method lookup_cached(struct klass *c, uint32_t sel)
{
    struct cache_entry *e = &c->cache[sel & 15];
    if (e->sel == sel) return e->fn;
    method m = lookup_slow(c, sel);
    *e = (struct cache_entry){ sel, m };
    return m;
}

uint64_t m_sel_slow(void)        /* 셀렉터, 캐시 없음: 목록을 끝까지 훑는다 */
{ uint64_t s = 0; EACH s += lookup_slow(arena[i].cls, SEL_APPLY)(&arena[i]); return s; }

uint64_t m_sel_cache(void)       /* 셀렉터, 클래스별 캐시 */
{ uint64_t s = 0; EACH s += lookup_cached(arena[i].cls, SEL_APPLY)(&arena[i]); return s; }

uint64_t m_sel_ic(void)          /* 셀렉터, 호출 자리의 한 칸 인라인 캐시 */
{
    uint64_t s = 0;
    struct { const struct klass *cls; method fn; } ic = {};
    EACH {
        struct klass *c = arena[i].cls;
        if (c != ic.cls) { ic.cls = c; ic.fn = lookup_slow(c, SEL_APPLY); }   /* 놓침 */
        s += ic.fn(&arena[i]);
    }
    return s;
}

uint64_t m_sel_pic(void)         /* 셀렉터, 네 칸 다형 인라인 캐시 */
{
    uint64_t s = 0;
    struct { const struct klass *cls; method fn; } pic[4] = {};
    uint32_t next = 0;
    EACH {
        struct klass *c = arena[i].cls;
        method m;
        if      (pic[0].cls == c) m = pic[0].fn;
        else if (pic[1].cls == c) m = pic[1].fn;
        else if (pic[2].cls == c) m = pic[2].fn;
        else if (pic[3].cls == c) m = pic[3].fn;
        else {                                       /* 놓침: 찾고, 한 칸을 바꾼다 */
            m = lookup_slow(c, SEL_APPLY);
            pic[next].cls = c;
            pic[next].fn = m;
            next = (next + 1) & 3;
        }
        s += m(&arena[i]);
    }
    return s;
}

uint64_t m_inlined(void)         /* 다시 switch --- 이번에는 몸통을 인라인해도 된다 */
{
    uint64_t s = 0;
    EACH {
        const struct obj *o = &arena[i];
        switch (o->kind) {
        case 0: s += g0(o); break; case 1: s += g1(o); break;
        case 2: s += g2(o); break; case 3: s += g3(o); break;
        case 4: s += g4(o); break; case 5: s += g5(o); break;
        case 6: s += g6(o); break; case 7: s += g7(o); break;
        }
    }
    return s;
}
