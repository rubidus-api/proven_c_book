#include <proven.h>
#include <stdio.h>

/* 이 함수는 어디서 기억을 얻는지 모른다 — 받은 할당자에게 물을 뿐이다.
   시그니처에 allocator 가 있다는 사실 자체가 "할당한다"는 선언이다. */
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

    proven_u8str_destroy(alloc, &s);   /* 준 곳으로 돌려준다 */
    return PROVEN_OK;
}

int main(void)
{
    /* ① 힙 — malloc/free 를 감싼 것 */
    printf("heap  :\n");
    (void)build(proven_heap_allocator(), "heap");

    /* ② 아레나 — 정적 버퍼 위에서 잘라 쓴다. 힙이 아예 없어도 된다 */
    static proven_byte_t backing[512];
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = sizeof backing });

    printf("arena :\n");
    (void)build(proven_arena_as_allocator(&arena), "arena");
    printf("  used %zu of %zu bytes\n", arena.offset, sizeof backing);

    /* 아레나는 개별 해제가 없다 — 한 번에 되돌린다 */
    proven_arena_reset(&arena);
    printf("  after reset: %zu bytes used\n", arena.offset);

    /* ③ 같은 아레나를 다시 쓴다 — 같은 코드, 다른 기억의 출처 */
    printf("arena :\n");
    (void)build(proven_arena_as_allocator(&arena), "arena again");
    return 0;
}
