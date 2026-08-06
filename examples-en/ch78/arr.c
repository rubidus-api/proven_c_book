#include <proven.h>
#include <stdio.h>

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();

    proven_result_array_t made = PROVEN_ARRAY_INIT(alloc, int, 4);
    if (!proven_is_ok(made.err)) {
        printf("array creation failed\n");
        return 1;
    }
    proven_array_t arr = made.value;

    for (int i = 1; i <= 6; i += 1) {          /* past a capacity of 4 it grows by itself */
        if (!proven_is_ok(PROVEN_ARRAY_PUSH(&arr, int, i * i))) {
            printf("push failed\n");
            PROVEN_ARRAY_DESTROY(&arr);
            return 1;
        }
    }

    printf("count: %zu\n", arr.len);
    for (size_t i = 0; i < arr.len; i += 1) {
        printf("%d ", *PROVEN_ARRAY_GET(&arr, int, i));
    }
    printf("\n");

    PROVEN_ARRAY_DESTROY(&arr);
    return 0;
}
