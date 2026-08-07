#include <proven.h>
#include <stdio.h>

int main(void)
{
    /* 아레나: 뒤를 받쳐 줄 기억을 통째로 주고, 그 안에서 잘라 쓴다 */
    static unsigned char backing[1024];
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = sizeof backing });

    proven_result_mem_mut_t a = proven_arena_alloc(&arena, 100);
    proven_result_mem_mut_t b = proven_arena_alloc(&arena, 200);

    if (!proven_is_ok(a.err) || !proven_is_ok(b.err)) {
        printf("arena allocation failed\n");
        return 1;
    }
    printf("carved two blocks: %zu, %zu bytes\n", a.value.size, b.value.size);

    /* 개별 해제는 없다 — 수명이 같은 것들을 한 번에 되돌린다 */
    proven_arena_reset(&arena);
    printf("one reset reclaimed everything\n");

    /* 그릇보다 큰 요청은 실패가 값으로 온다 (붕괴하지 않는다) */
    proven_result_mem_mut_t too_big = proven_arena_alloc(&arena, 100000);
    printf("oversized request: %s\n", proven_is_ok(too_big.err) ? "granted" : "refused");
    return 0;
}
