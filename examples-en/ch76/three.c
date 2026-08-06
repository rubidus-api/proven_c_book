/* The same code, three sources of memory — the heap, an arena, a pool.
   What it makes possible that an allocator is simply a value. */
#include <proven.h>

/* This function does not know where the memory it uses comes from.
   Taking an allocator as a parameter is itself "it may allocate", written in
   the signature. */
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
    proven_u8str_destroy(alloc, &s);      /* through the very allocator it was made with */
}

int main(void)
{
    /* ── (1) the heap — standard malloc wrapped in the interface ──── */
    run("heap", proven_heap_allocator());

    /* ── (2) an arena — on a static array. malloc is never called ─── */
    static proven_byte_t backing[512];
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = sizeof backing });
    run("arena", proven_arena_as_allocator(&arena));

    proven_println("  arena in use: {} / {} bytes",
                   PROVEN_ARG(arena.offset), PROVEN_ARG(arena.backing.size));

    /* an arena is released not piece by piece but whole */
    proven_arena_reset(&arena);
    proven_println("  in use after the reset: {} (individual frees did nothing)",
                   PROVEN_ARG(arena.offset));

    /* ── (3) a pool — pieces of one size, handed round ───────────── */
    proven_pool_t pool;
    proven_err_t pe = proven_pool_init(&pool, proven_heap_allocator(),
                                       /* item_size  */ 129,
                                       /* item_align */ PROVEN_MAX_ALIGN,
                                       /* bin_cap    */ 8);
    if (proven_is_ok(pe)) {
        proven_allocator_t pa = proven_pool_as_allocator(&pool);
        run("pool", pa);
        run("pool", pa);          /* the second reuses the piece given back a moment ago */
        proven_pool_destroy(&pool);
    }

    /* ── (4) when an arena runs dry — the failure arrives as a value ─ */
    proven_arena_t tiny = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = 64 });
    proven_u8str_t s;
    proven_err_t e = make_list(proven_arena_as_allocator(&tiny), 6, &s);
    proven_println("a 64-byte arena -> code {} (a refusal, not a collapse)",
                   PROVEN_ARG((int)e));
    return 0;
}
