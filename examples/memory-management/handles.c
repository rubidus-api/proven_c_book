/* 세대 번호를 단 핸들 --- 댕글링 포인터를 *검출할 수 있는* 참조.
 * 포인터 대신 (칸 번호, 세대)를 건넨다. 칸을 돌려받을 때마다 그 칸의 세대를 올리므로,
 * 옛 핸들은 세대가 맞지 않아 「이미 죽은 객체」로 판정된다. 칸은 다시 쓰여도 된다. */
#include <stdint.h>
#include <stdio.h>

enum { SLOTS = 4 };

typedef struct { uint32_t index, gen; } handle;

typedef struct {
    double   value[SLOTS];
    uint32_t gen[SLOTS];        /* 칸마다의 세대 */
    bool     used[SLOTS];
} pool;

static bool pool_new(pool *p, double v, handle *out)
{
    for (uint32_t i = 0; i < SLOTS; i++) {
        if (!p->used[i]) {
            p->used[i] = true;
            p->value[i] = v;
            *out = (handle){ i, p->gen[i] };
            return true;
        }
    }
    return false;               /* 빈칸이 없다 */
}

static void pool_free(pool *p, handle h)
{
    if (h.index < SLOTS && p->used[h.index] && p->gen[h.index] == h.gen) {
        p->used[h.index] = false;
        p->gen[h.index]++;      /* ★ 이 칸을 가리키던 모든 핸들이 이 순간 낡는다 */
    }
}

/* 살아 있는 객체면 그 자리를, 아니면 NULL 을 돌려준다 --- 쓰기 전에 *물어볼 수* 있다. */
static double *pool_get(pool *p, handle h)
{
    if (h.index >= SLOTS || !p->used[h.index] || p->gen[h.index] != h.gen)
        return NULL;
    return &p->value[h.index];
}

static void show(pool *p, const char *name, handle h)
{
    double *v = pool_get(p, h);
    if (v) printf("%-6s (slot %u, gen %u) -> %.1f\n", name, (unsigned)h.index, (unsigned)h.gen, *v);
    else   printf("%-6s (slot %u, gen %u) -> stale handle, object is gone\n",
                  name, (unsigned)h.index, (unsigned)h.gen);
}

int main(void)
{
    pool p = {0};
    handle a, b;
    if (!pool_new(&p, 1.5, &a)) return 1;
    show(&p, "a", a);

    pool_free(&p, a);               /* a 가 가리키던 객체를 돌려준다 */
    if (!pool_new(&p, 9.0, &b)) return 1;  /* 같은 칸이 다시 쓰인다 */
    show(&p, "b", b);
    show(&p, "a", a);               /* 포인터였다면 b 의 값을 읽었을 자리 */
    return 0;
}
