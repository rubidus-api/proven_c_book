/* 실패 경로에서 자원을 흘리지 않는 법 — 한 곳에 모아 역순으로 정리한다.
   70장에서 본 `goto cleanup` 관용구가 proven 의 에러 값과 만나는 자리다. */
#include <proven.h>

/* 실패를 흉내 내기 위한 할당자: n 번째 할당부터는 반드시 실패한다. */
typedef struct {
    proven_allocator_t base;
    int                budget;   /* 남은 성공 횟수 */
} failing_ctx_t;

static proven_result_mem_mut_t failing_alloc(void *ctx, proven_size_t size,
                                             proven_size_t align)
{
    failing_ctx_t *f = (failing_ctx_t *)ctx;
    if (f->budget <= 0)
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_NOMEM };
    f->budget--;
    return f->base.alloc_fn(f->base.ctx, size, align);
}

static proven_result_mem_mut_t failing_realloc(void *ctx, void *old_ptr,
                                               proven_size_t old_size,
                                               proven_size_t new_size,
                                               proven_size_t align)
{
    failing_ctx_t *f = (failing_ctx_t *)ctx;
    if (f->budget <= 0)
        return (proven_result_mem_mut_t){ .err = PROVEN_ERR_NOMEM };
    f->budget--;
    return f->base.realloc_fn(f->base.ctx, old_ptr, old_size, new_size, align);
}

static void failing_free(void *ctx, void *ptr)
{
    failing_ctx_t *f = (failing_ctx_t *)ctx;
    f->base.free_fn(f->base.ctx, ptr);
}

/* 자원 둘을 잡고 일하는 함수. 어디서 실패하든 잡은 것만 정확히 되돌린다. */
static proven_err_t join_two(proven_allocator_t alloc,
                             proven_u8str_view_t a,
                             proven_u8str_view_t b,
                             proven_u8str_t *out)
{
    proven_err_t   err   = PROVEN_OK;
    bool           has_x = false;
    proven_u8str_t x;                       /* 첫 번째 자원 */
    proven_u8str_t y;                       /* 두 번째 자원 */

    proven_result_u8str_t rx = proven_u8str_create_from_view(alloc, a);
    if (!proven_is_ok(rx.err)) { err = rx.err; goto done; }
    x = rx.value; has_x = true;

    proven_result_u8str_t ry = proven_u8str_create_from_view(alloc, b);
    if (!proven_is_ok(ry.err)) { err = ry.err; goto done; }
    y = ry.value;

    /* 두 조각을 이어 붙인다 — 모자라면 늘린다(할당자가 필요한 이유) */
    err = proven_u8str_append_grow(alloc, &y, proven_u8str_as_view(&x));
    if (!proven_is_ok(err)) { proven_u8str_destroy(alloc, &y); goto done; }

    *out = y;                               /* 성공: y 의 소유권을 넘긴다 */
                                            /* x 는 아래에서 반납된다 */
done:
    if (has_x) proven_u8str_destroy(alloc, &x);   /* 잡았으면 반드시 되돌린다 */
    return err;
}

static void run(int budget)
{
    proven_allocator_t heap = proven_heap_allocator();
    failing_ctx_t      ctx  = { .base = heap, .budget = budget };
    proven_allocator_t alloc = {
        .ctx = &ctx, .alloc_fn = failing_alloc,
        .realloc_fn = failing_realloc, .free_fn = failing_free,
    };

    proven_u8str_t out;
    proven_err_t e = join_two(alloc, PROVEN_LIT("world"), PROVEN_LIT("hello, "), &out);

    if (proven_is_ok(e)) {
        proven_println("budget={} -> ok: \"{}\"  (남은 예산 {})",
                       PROVEN_ARG(budget), PROVEN_ARG(proven_u8str_as_view(&out)),
                       PROVEN_ARG(ctx.budget));
        proven_u8str_destroy(alloc, &out);
    } else {
        proven_println("budget={} -> 실패(코드 {}) — 잡았던 것은 전부 반납됐다",
                       PROVEN_ARG(budget), PROVEN_ARG((int)e));
    }
}

int main(void)
{
    run(0);   /* 첫 할당부터 실패 */
    run(1);   /* 첫 자원은 잡고 두 번째에서 실패 → x 를 흘리면 누수다 */
    run(9);   /* 전부 성공 */
    return 0;
}
