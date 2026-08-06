#include <proven.h>
#include <stdio.h>

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();

    /* an owning string: made with the capacity stated */
    proven_result_u8str_t made = proven_u8str_create(alloc, 64);
    if (!proven_is_ok(made.err)) {
        printf("string creation failed\n");
        return 1;
    }
    proven_u8str_t s = made.value;

    /* appending can fail too — it is checked as a value */
    proven_err_t e = proven_u8str_append(&s, proven_u8str_view_from_cstr("Hello, "));
    if (proven_is_ok(e)) {
        e = proven_u8str_append(&s, proven_u8str_view_from_cstr("world!"));
    }
    if (!proven_is_ok(e)) {
        printf("append failed\n");
        proven_u8str_destroy(alloc, &s);
        return 1;
    }

    /* a view: a pair of pointer and length — the length is known without counting */
    proven_u8str_view_t v = proven_u8str_as_view(&s);
    printf("content: %.*s\n", (int)v.size, (const char *)v.ptr);
    printf("byte length: %zu (not character count)\n", v.size);

    proven_u8str_destroy(alloc, &s);
    return 0;
}
