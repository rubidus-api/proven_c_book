/* 네 컨테이너의 한살이 — 만들고, 넣고, 훑고, 되돌린다.
   array · list · ring · map 을 한 화면에서 비교한다. */
#include <proven.h>

/* 침습적 리스트: 노드가 자료 안에 산다 */
typedef struct {
    int                id;
    proven_list_node_t link;      /* ← 이 한 칸이 리스트에 매달릴 고리다 */
} task_t;

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();

    /* ── ① 자라는 배열 ───────────────────────────────────────── */
    proven_result_array_t ar = PROVEN_ARRAY_INIT(alloc, int, 2);
    if (!proven_is_ok(ar.err)) return 1;
    proven_array_t arr = ar.value;

    for (int i = 1; i <= 5; i++)
        (void)PROVEN_ARRAY_PUSH(&arr, int, i * i);

    proven_println("array  len={} cap={} elem_size={}",
                   PROVEN_ARG(arr.len), PROVEN_ARG(arr.cap), PROVEN_ARG(arr.elem_size));

    /* 훑기는 인덱스로 — 포인터를 들고 다니지 않는다 */
    proven_print("       내용:");
    for (proven_size_t i = 0; i < arr.len; i++)
        proven_print(" {}", PROVEN_ARG(*PROVEN_ARRAY_GET(&arr, int, i)));
    proven_println("");

    int last = 0;
    (void)PROVEN_ARRAY_POP(&arr, int, &last);
    proven_println("       pop -> {} (len {})", PROVEN_ARG(last), PROVEN_ARG(arr.len));
    PROVEN_ARRAY_DESTROY(&arr);

    /* ── ② 침습적 리스트 — 할당이 한 번도 없다 ───────────────── */
    task_t     items[3] = { { .id = 10 }, { .id = 20 }, { .id = 30 } };
    proven_list_t list;
    proven_list_init(&list);
    for (int i = 0; i < 3; i++)
        proven_list_push_back(&list, &items[i].link);

    proven_list_node_t *node, *tmp;
    proven_print("list   앞에서부터:");
    PROVEN_LIST_FOR_EACH(node, &list) {
        task_t *t = PROVEN_LIST_ENTRY(node, task_t, link);
        proven_print(" {}", PROVEN_ARG(t->id));
    }
    proven_println("  (노드가 자료 안에 있으므로 할당이 필요 없다)");

    /* 훑으면서 떼어내려면 SAFE 판을 쓴다 */
    PROVEN_LIST_FOR_EACH_SAFE(node, tmp, &list) {
        task_t *t = PROVEN_LIST_ENTRY(node, task_t, link);
        if (t->id == 20) proven_list_remove(node);
    }
    proven_print("       20 을 뗀 뒤:");
    PROVEN_LIST_FOR_EACH(node, &list) {
        proven_print(" {}", PROVEN_ARG(PROVEN_LIST_ENTRY(node, task_t, link)->id));
    }
    proven_println("");

    /* ── ③ 링 버퍼 — 고정 크기, 지나간 것은 버린다 ───────────── */
    proven_result_ring_t rr = PROVEN_RING_INIT(alloc, int, 4);
    if (proven_is_ok(rr.err)) {
        proven_ring_t ring = rr.value;
        for (int i = 1; i <= 4; i++) (void)proven_ring_push(&ring, &i);

        int five = 5;
        proven_err_t full = proven_ring_push(&ring, &five);
        proven_println("ring   가득 찬 뒤 push -> err={} (덮지 않고 알린다)",
                       PROVEN_ARG((int)full));

        int v = 0;
        (void)proven_ring_pop(&ring, &v);
        proven_println("       pop -> {} (가장 먼저 넣은 것)", PROVEN_ARG(v));
        proven_ring_destroy(&ring);
    }

    /* ── ④ 해시 맵 — 키를 소유할지 빌릴지 고른다 ─────────────── */
    /* 키를 맵이 복사해 소유한다(U8_OWNED) — 빌리는 판도 있다 */
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
        proven_println("       get(\"없음\") -> {} (없으면 널)",
                       PROVEN_ARG((bool)(proven_map_get(&map,
                            (proven_map_key_t){ .str = PROVEN_LIT("없음") }) == nullptr)));
        proven_map_destroy(&map);
    }
    return 0;
}
