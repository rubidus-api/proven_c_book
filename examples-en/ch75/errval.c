#include <proven.h>
#include <stdio.h>

/* Work that can fail: two pieces appended to a string of a settled capacity.
   The error comes back as a value, and the caller passes it upward as it is. */
static proven_err_t make_greeting(proven_allocator_t alloc,
                                  const char *who,
                                  proven_size_t cap,
                                  proven_u8str_t *out)
{
    proven_result_u8str_t made = proven_u8str_create(alloc, cap);
    if (!proven_is_ok(made.err))
        return made.err;                 /* the failure is handed upward */

    proven_u8str_t s = made.value;       /* value is used only after the check */

    proven_err_t e = proven_u8str_append(&s, proven_u8str_view_from_cstr("Hello, "));
    if (proven_is_ok(e))
        e = proven_u8str_append(&s, proven_u8str_view_from_cstr(who));
    if (!proven_is_ok(e)) {
        proven_u8str_destroy(alloc, &s); /* having failed, the cleaning up is ours too */
        return e;
    }

    *out = s;
    return PROVEN_OK;
}

static void try(proven_allocator_t alloc, const char *who, proven_size_t cap)
{
    proven_u8str_t s;
    proven_err_t e = make_greeting(alloc, who, cap, &s);

    if (!proven_is_ok(e)) {
        printf("cap=%2zu who=%-8s -> failed with error code %d\n",
               cap, who, (int)e);
        return;
    }
    proven_u8str_view_t v = proven_u8str_as_view(&s);
    printf("cap=%2zu who=%-8s -> \"%.*s\"\n",
           cap, who, (int)v.size, (const char *)v.ptr);
    proven_u8str_destroy(alloc, &s);
}

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();

    try(alloc, "world", 32);   /* roomy */
    try(alloc, "world", 8);    /* not enough room — it refuses rather than truncating */

    printf("PROVEN_OK is %d, and every failure is non-zero\n", (int)PROVEN_OK);
    return 0;
}
