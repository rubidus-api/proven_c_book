#include <proven.h>
#include <stdio.h>

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();

    /* 소유하는 문자열: 용량을 명시해 만든다 */
    proven_result_u8str_t made = proven_u8str_create(alloc, 64);
    if (!proven_is_ok(made.err)) {
        printf("string creation failed\n");
        return 1;
    }
    proven_u8str_t s = made.value;

    /* 덧붙이기도 실패할 수 있다 — 값으로 확인한다 */
    proven_err_t e = proven_u8str_append(&s, proven_u8str_view_from_cstr("Hello, "));
    if (proven_is_ok(e)) {
        e = proven_u8str_append(&s, proven_u8str_view_from_cstr("world!"));
    }
    if (!proven_is_ok(e)) {
        printf("append failed\n");
        proven_u8str_destroy(alloc, &s);
        return 1;
    }

    /* view: 포인터와 길이의 쌍 — 길이를 세지 않아도 안다 */
    proven_u8str_view_t v = proven_u8str_as_view(&s);
    printf("content: %.*s\n", (int)v.size, (const char *)v.ptr);
    printf("byte length: %zu (not character count)\n", v.size);

    proven_u8str_destroy(alloc, &s);
    return 0;
}
