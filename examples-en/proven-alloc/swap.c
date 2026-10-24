#include <proven.h>
#include <stdio.h>

/* This function does not know where the memory comes from — it only asks the
   allocator it was given. That an allocator is in the signature is itself the
   declaration "this allocates". */
static proven_err_t build(proven_allocator_t alloc, const char *who)
{
    proven_result_u8str_t made = proven_u8str_create(alloc, 64);
    if (!proven_is_ok(made.err)) return made.err;

    proven_u8str_t s = made.value;
    proven_err_t e = proven_u8str_append(&s, proven_u8str_view_from_cstr("hi, "));
    if (proven_is_ok(e))
        e = proven_u8str_append(&s, proven_u8str_view_from_cstr(who));
    if (!proven_is_ok(e)) { proven_u8str_destroy(alloc, &s); return e; }

    proven_u8str_view_t v = proven_u8str_as_view(&s);
    printf("  -> \"%.*s\"\n", (int)v.size, (const char *)v.ptr);

    proven_u8str_destroy(alloc, &s);   /* given back to where it came from */
    return PROVEN_OK;
}

int main(void)
{
    /* (1) the heap — malloc/free wrapped */
    printf("heap  :\n");
    (void)build(proven_heap_allocator(), "heap");

    /* (2) an arena — carved out of a static buffer. There need be no heap at all */
    static proven_byte_t backing[512];
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = sizeof backing });

    printf("arena :\n");
    (void)build(proven_arena_as_allocator(&arena), "arena");
    printf("  used %zu of %zu bytes\n", arena.offset, sizeof backing);

    /* an arena has no individual free — it is given back at once */
    proven_arena_reset(&arena);
    printf("  after reset: %zu bytes used\n", arena.offset);

    /* (3) the same arena used again — the same code, a different source of memory */
    printf("arena :\n");
    (void)build(proven_arena_as_allocator(&arena), "arena again");
    return 0;
}
