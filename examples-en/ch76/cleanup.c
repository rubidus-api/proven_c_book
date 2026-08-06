/* How not to leak a resource on a failure path — everything gathered in one
   place and released in reverse. The `goto cleanup` idiom of chapter 65 meeting
   proven's error values. */
#include <proven.h>

/* An allocator for imitating failure: from the nth allocation on it always fails. */
typedef struct {
    proven_allocator_t base;
    int                budget;   /* how many successes are left */
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

/* A function holding two resources. Wherever it fails, exactly what was taken is given back. */
static proven_err_t join_two(proven_allocator_t alloc,
                             proven_u8str_view_t a,
                             proven_u8str_view_t b,
                             proven_u8str_t *out)
{
    proven_err_t   err   = PROVEN_OK;
    bool           has_x = false;
    proven_u8str_t x;                       /* the first resource */
    proven_u8str_t y;                       /* the second resource */

    proven_result_u8str_t rx = proven_u8str_create_from_view(alloc, a);
    if (!proven_is_ok(rx.err)) { err = rx.err; goto done; }
    x = rx.value; has_x = true;

    proven_result_u8str_t ry = proven_u8str_create_from_view(alloc, b);
    if (!proven_is_ok(ry.err)) { err = ry.err; goto done; }
    y = ry.value;

    /* the two pieces joined — it grows if there is not enough room (why an allocator is needed) */
    err = proven_u8str_append_grow(alloc, &y, proven_u8str_as_view(&x));
    if (!proven_is_ok(err)) { proven_u8str_destroy(alloc, &y); goto done; }

    *out = y;                               /* success: ownership of y is handed over */
                                            /* x is given back below */
done:
    if (has_x) proven_u8str_destroy(alloc, &x);   /* if it was taken, it is always given back */
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
        proven_println("budget={} -> ok: \"{}\"  (budget left {})",
                       PROVEN_ARG(budget), PROVEN_ARG(proven_u8str_as_view(&out)),
                       PROVEN_ARG(ctx.budget));
        proven_u8str_destroy(alloc, &out);
    } else {
        proven_println("budget={} -> failed (code {}) — everything taken was given back",
                       PROVEN_ARG(budget), PROVEN_ARG((int)e));
    }
}

int main(void)
{
    run(0);   /* it fails from the first allocation */
    run(1);   /* the first resource is taken and the second fails -> letting x slip is a leak */
    run(9);   /* everything succeeds */
    return 0;
}
