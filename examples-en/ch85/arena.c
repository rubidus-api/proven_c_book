#include <proven.h>
#include <stdio.h>

int main(void)
{
    /* An arena: the backing memory is given whole, and pieces are carved out of it */
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

    /* there is no individual free — things of the same lifetime are given back at once */
    proven_arena_reset(&arena);
    printf("one reset reclaimed everything\n");

    /* a request larger than the vessel comes back as a failure value (it does not collapse) */
    proven_result_mem_mut_t too_big = proven_arena_alloc(&arena, 100000);
    printf("oversized request: %s\n", proven_is_ok(too_big.err) ? "granted" : "refused");
    return 0;
}
