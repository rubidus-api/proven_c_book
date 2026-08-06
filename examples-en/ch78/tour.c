/* The life of four containers — made, filled, walked and given back.
   array, list, ring and map compared on one screen. */
#include <proven.h>

/* an intrusive list: the node lives inside the data */
typedef struct {
    int                id;
    proven_list_node_t link;      /* <- this one slot is the hook it hangs on the list by */
} task_t;

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();

    /* ── (1) a growing array ─────────────────────────────────────── */
    proven_result_array_t ar = PROVEN_ARRAY_INIT(alloc, int, 2);
    if (!proven_is_ok(ar.err)) return 1;
    proven_array_t arr = ar.value;

    for (int i = 1; i <= 5; i++)
        (void)PROVEN_ARRAY_PUSH(&arr, int, i * i);

    proven_println("array  len={} cap={} elem_size={}",
                   PROVEN_ARG(arr.len), PROVEN_ARG(arr.cap), PROVEN_ARG(arr.elem_size));

    /* walked by index — no pointer is carried around */
    proven_print("       content:");
    for (proven_size_t i = 0; i < arr.len; i++)
        proven_print(" {}", PROVEN_ARG(*PROVEN_ARRAY_GET(&arr, int, i)));
    proven_println("");

    int last = 0;
    (void)PROVEN_ARRAY_POP(&arr, int, &last);
    proven_println("       pop -> {} (len {})", PROVEN_ARG(last), PROVEN_ARG(arr.len));
    PROVEN_ARRAY_DESTROY(&arr);

    /* ── (2) an intrusive list — not one allocation ──────────────── */
    task_t     items[3] = { { .id = 10 }, { .id = 20 }, { .id = 30 } };
    proven_list_t list;
    proven_list_init(&list);
    for (int i = 0; i < 3; i++)
        proven_list_push_back(&list, &items[i].link);

    proven_list_node_t *node, *tmp;
    proven_print("list   from the front:");
    PROVEN_LIST_FOR_EACH(node, &list) {
        task_t *t = PROVEN_LIST_ENTRY(node, task_t, link);
        proven_print(" {}", PROVEN_ARG(t->id));
    }
    proven_println("  (the node is inside the data, so no allocation is needed)");

    /* to unhook while walking, the SAFE version is used */
    PROVEN_LIST_FOR_EACH_SAFE(node, tmp, &list) {
        task_t *t = PROVEN_LIST_ENTRY(node, task_t, link);
        if (t->id == 20) proven_list_remove(node);
    }
    proven_print("       after removing 20:");
    PROVEN_LIST_FOR_EACH(node, &list) {
        proven_print(" {}", PROVEN_ARG(PROVEN_LIST_ENTRY(node, task_t, link)->id));
    }
    proven_println("");

    /* ── (3) a ring buffer — fixed size, the past is dropped ─────── */
    proven_result_ring_t rr = PROVEN_RING_INIT(alloc, int, 4);
    if (proven_is_ok(rr.err)) {
        proven_ring_t ring = rr.value;
        for (int i = 1; i <= 4; i++) (void)proven_ring_push(&ring, &i);

        int five = 5;
        proven_err_t full = proven_ring_push(&ring, &five);
        proven_println("ring   push when full -> err={} (it reports rather than overwrites)",
                       PROVEN_ARG((int)full));

        int v = 0;
        (void)proven_ring_pop(&ring, &v);
        proven_println("       pop -> {} (the one put in first)", PROVEN_ARG(v));
        proven_ring_destroy(&ring);
    }

    /* ── (4) a hash map — the key is owned or borrowed, as chosen ── */
    /* the map copies and owns the key (U8_OWNED) — a borrowing version exists too */
    proven_result_map_t mr = PROVEN_MAP_INIT_U8_OWNED(alloc, int, 8);
    if (proven_is_ok(mr.err)) {
        proven_map_t map = mr.value;
        int a = 1, b = 2;
        (void)proven_map_set_u8_owned(&map, PROVEN_LIT("alpha"), &a);
        (void)proven_map_set_u8_owned(&map, PROVEN_LIT("beta"), &b);

        const int *found = proven_map_get(&map,
                        (proven_map_key_t){ .str = PROVEN_LIT("beta") });
        proven_println("map    get(\"beta\") -> {}",
                       PROVEN_ARG(found ? *found : -1));
        proven_println("       get(\"absent\") -> {} (null when there is none)",
                       PROVEN_ARG((bool)(proven_map_get(&map,
                            (proven_map_key_t){ .str = PROVEN_LIT("absent") }) == nullptr)));
        proven_map_destroy(&map);
    }
    return 0;
}
