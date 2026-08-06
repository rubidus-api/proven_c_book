#include <proven.h>
#include <stdio.h>

/* 낱말을 세어 정렬해 찍는다 — 맵과 배열과 정렬을 한 번에 */
typedef struct { proven_u8str_view_t word; int count; } entry_t;

/* 비교자는 전순서여야 한다: 동점도 일관되게 갈라 준다 (50장의 반례를 피한다) */
static int by_count_desc(const void *a, const void *b)
{
    const entry_t *x = a, *y = b;
    if (x->count != y->count) return (x->count < y->count) - (x->count > y->count);

    proven_size_t n = x->word.size < y->word.size ? x->word.size : y->word.size;
    int c = proven_memcmp(x->word.ptr, y->word.ptr, n);
    if (c != 0) return c;
    return (x->word.size > y->word.size) - (x->word.size < y->word.size);
}

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();
    const char *text = "the quick fox the lazy dog the fox";

    /* 문자열 키 맵: 기본은 HashDoS 에 견디는 키드 해시를 쓴다 */
    proven_result_map_t made_map =
        proven_map_create(alloc, 16, PROVEN_KEY_TYPE_U8_OWNED, sizeof(int), alignof(int));
    if (!proven_is_ok(made_map.err)) return 1;
    proven_map_t counts = made_map.value;

    proven_u8str_view_t all = proven_u8str_view_from_cstr(text);
    proven_u8str_view_t space = proven_u8str_view_from_cstr(" ");
    proven_size_t start = 0;

    for (;;) {
        proven_size_t hit = proven_u8str_view_find(all, start, space);
        proven_size_t end = (hit == PROVEN_INDEX_NOT_FOUND) ? all.size : hit;
        proven_u8str_view_t w = proven_u8str_view_slice(all, start, end - start);

        const int *seen = proven_map_get(&counts, (proven_map_key_t){ .str = w });
        int next = seen ? *seen + 1 : 1;
        if (!proven_is_ok(proven_map_set(&counts, (proven_map_key_t){ .str = w }, &next)))
            break;

        if (hit == PROVEN_INDEX_NOT_FOUND) break;
        start = hit + 1;
    }
    printf("distinct words: %zu\n", counts.len);

    /* 세어 둔 것을 배열에 모아 정렬한다 */
    proven_result_array_t made_arr = PROVEN_ARRAY_INIT(alloc, entry_t, 8);
    if (!proven_is_ok(made_arr.err)) return 1;
    proven_array_t list = made_arr.value;

    const char *words[] = {"the", "quick", "fox", "lazy", "dog"};
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) {
        proven_u8str_view_t w = proven_u8str_view_from_cstr(words[i]);
        const int *c = proven_map_get(&counts, (proven_map_key_t){ .str = w });
        entry_t e = { .word = w, .count = c ? *c : 0 };
        if (!proven_is_ok(PROVEN_ARRAY_PUSH(&list, entry_t, e))) break;
    }

    proven_array_sort(&list, by_count_desc);   /* 최악에도 O(n log n) 보장 */
    for (size_t i = 0; i < list.len; i++) {
        const entry_t *e = PROVEN_ARRAY_GET(&list, entry_t, i);
        printf("  %.*s = %d\n", (int)e->word.size, (const char *)e->word.ptr, e->count);
    }

    PROVEN_ARRAY_DESTROY(&list);
    proven_map_destroy(&counts);
    return 0;
}
