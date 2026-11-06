/* ways.c(함수를 고르는 길들)와 harness.c(차리기, 재기)가 함께 쓴다. */
#include <stdint.h>

#define N    (1u << 16)          /* 객체 수: 아레나 2 MiB, 큰 캐시 안에 든다 */
#define REPS 8                   /* 한 번 잴 때 아레나를 걷는 횟수 */
#define KMAX 8

struct obj;
typedef uint64_t (*method)(const struct obj *);

struct ops { method apply; method other; };                /* 가상 함수 표 */
struct cache_entry { uint32_t sel; method fn; };
struct method_entry { uint32_t sel; method fn; };
struct klass {                                        /* 셀렉터로 찾는 클래스 */
    const struct klass *parent;
    const struct method_entry *methods;
    uint32_t n;
    struct cache_entry cache[16];                           /* 클래스별 캐시 */
};
struct obj {
    uint64_t x;
    uint32_t kind;
    const struct ops *sops;      /* static const 표를 가리킨다 */
    const struct ops *dops;      /* 실행 때 힙에 지은 표를 가리킨다 */
    struct klass *cls;
};
struct fat { const struct obj *data; const struct ops *ops; };   /* 뚱뚱한 포인터 */

enum { SEL_APPLY = 8 };          /* 셀렉터 1~7 은 다른 메서드, apply 는 맨 끝 */

extern struct obj arena[N];
extern struct fat fats[N];
extern uint32_t *by_kind[KMAX], count[KMAX], kinds;
extern const method fs[KMAX];
extern const struct ops static_ops[KMAX];
uint64_t other(const struct obj *o);

uint64_t m_direct(void), m_grouped(void), m_switch(void), m_binary(void), m_static(void),
         m_dynamic(void), m_fat(void), m_inlined(void), m_sel_slow(void),
         m_sel_cache(void), m_sel_ic(void), m_sel_pic(void);
