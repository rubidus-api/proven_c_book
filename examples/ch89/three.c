/* 같은 코드, 세 가지 기억의 출처 — 힙, 아레나, 풀.
   할당자가 그냥 값이라는 사실이 무엇을 가능하게 하는지 본다. */
#include <proven.h>

/* 이 함수는 자기가 쓰는 기억이 어디서 오는지 모른다.
   할당자를 인자로 받았다는 사실만으로 "할당할 수 있다"가 시그니처에 드러난다. */
static proven_err_t make_list(proven_allocator_t alloc, int n, proven_u8str_t *out)
{
    proven_result_u8str_t made = proven_u8str_create(alloc, 128);
    if (!proven_is_ok(made.err)) return made.err;

    proven_u8str_t s = made.value;
    for (int i = 1; i <= n; i++) {
        proven_fmt_result_t r =
            proven_u8str_append_fmt(&s, i == 1 ? "{}" : ", {}", PROVEN_ARG(i));
        if (!proven_is_ok(r.err)) {
            proven_u8str_destroy(alloc, &s);
            return r.err;
        }
    }
    *out = s;
    return PROVEN_OK;
}

static void run(const char *where, proven_allocator_t alloc)
{
    proven_u8str_t s;
    proven_err_t e = make_list(alloc, 6, &s);
    if (!proven_is_ok(e)) {
        proven_println("{:<8} failed (code {})", PROVEN_ARG(where), PROVEN_ARG((int)e));
        return;
    }
    proven_println("{:<8} {}", PROVEN_ARG(where), PROVEN_ARG(proven_u8str_as_view(&s)));
    proven_u8str_destroy(alloc, &s);      /* 만들 때 준 그 할당자로 */
}

int main(void)
{
    /* ── ① 힙 — 표준 malloc 을 인터페이스로 감싼 것 ─────────────── */
    run("heap", proven_heap_allocator());

    /* ── ② 아레나 — 정적 배열 위에서. malloc 은 한 번도 불리지 않는다 ── */
    static proven_byte_t backing[512];
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = sizeof backing });
    run("arena", proven_arena_as_allocator(&arena));

    proven_println("  arena in use: {} / {} bytes",
                   PROVEN_ARG(arena.offset), PROVEN_ARG(arena.backing.size));

    /* 아레나의 해제는 개별이 아니라 통째로다 */
    proven_arena_reset(&arena);
    proven_println("  in use after reset: {} (freeing individually did nothing)",
                   PROVEN_ARG(arena.offset));

    /* ── ③ 풀 — 같은 크기 조각을 돌려 쓴다 ─────────────────────── */
    proven_pool_t pool;
    proven_err_t pe = proven_pool_init(&pool, proven_heap_allocator(),
                                       /* item_size  */ 129,
                                       /* item_align */ PROVEN_MAX_ALIGN,
                                       /* bin_cap    */ 8);
    if (proven_is_ok(pe)) {
        proven_allocator_t pa = proven_pool_as_allocator(&pool);
        run("pool", pa);
        run("pool", pa);          /* 두 번째는 앞서 반납된 조각을 재사용한다 */
        proven_pool_destroy(&pool);
    }

    /* ── ④ 아레나가 바닥나면 — 실패가 값으로 온다 ───────────────── */
    proven_arena_t tiny = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = 64 });
    proven_u8str_t s;
    proven_err_t e = make_list(proven_arena_as_allocator(&tiny), 6, &s);
    proven_println("a 64-byte arena -> code {} (refused, not a collapse)",
                   PROVEN_ARG((int)e));
    return 0;
}
