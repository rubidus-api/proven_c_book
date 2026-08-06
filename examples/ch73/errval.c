#include <proven.h>
#include <stdio.h>

/* 실패할 수 있는 일을 한다: 정해진 용량의 문자열에 두 조각을 붙인다.
   에러는 값으로 돌아오고, 호출자는 그것을 그대로 위로 전달한다. */
static proven_err_t make_greeting(proven_allocator_t alloc,
                                  const char *who,
                                  proven_size_t cap,
                                  proven_u8str_t *out)
{
    proven_result_u8str_t made = proven_u8str_create(alloc, cap);
    if (!proven_is_ok(made.err))
        return made.err;                 /* 실패를 위로 넘긴다 */

    proven_u8str_t s = made.value;       /* 확인한 뒤에야 value 를 쓴다 */

    proven_err_t e = proven_u8str_append(&s, proven_u8str_view_from_cstr("Hello, "));
    if (proven_is_ok(e))
        e = proven_u8str_append(&s, proven_u8str_view_from_cstr(who));
    if (!proven_is_ok(e)) {
        proven_u8str_destroy(alloc, &s); /* 실패했으면 뒷정리도 우리 몫 */
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

    try(alloc, "world", 32);   /* 넉넉하다 */
    try(alloc, "world", 8);    /* 자리가 모자란다 — 자르지 않고 거부한다 */

    printf("PROVEN_OK is %d, and every failure is non-zero\n", (int)PROVEN_OK);
    return 0;
}
