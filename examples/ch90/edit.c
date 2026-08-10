/* 문자열을 고치는 연산들 — 끼우기, 지우기, 바꾸기, 그리고 되쓰기.
   전부 "들어가면 하고, 안 들어가면 원본을 그대로 두고 거부한다". */
#include <proven.h>

static void show(const char *label, const proven_u8str_t *s)
{
    proven_u8str_view_t v = proven_u8str_as_view(s);
    proven_println("{:<16} \"{}\" (len {})",
                   PROVEN_ARG(label), PROVEN_ARG(v), PROVEN_ARG(v.size));
}

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();

    /* ── ① 뷰에서 바로 만들기 ─────────────────────────────────── */
    proven_result_u8str_t r =
        proven_u8str_create_from_view(alloc, PROVEN_LIT("hello world"));
    if (!proven_is_ok(r.err)) return 1;
    proven_u8str_t s = r.value;
    show("just created", &s);

    /* ── ② 가운데에 끼우기 ────────────────────────────────────── */
    proven_err_t e = proven_u8str_insert_grow(alloc, &s, 5, PROVEN_LIT(","));
    show("insert(5, \",\")", &s);

    /* ── ③ 구간 바꾸기 — 길이가 달라도 된다 ───────────────────── */
    e = proven_u8str_replace_at_grow(alloc, &s, 7, 5, PROVEN_LIT("proven C"));
    show("replace_at", &s);

    /* ── ④ 처음 만나는 조각 바꾸기 ────────────────────────────── */
    e = proven_u8str_replace_first(&s, 0, PROVEN_LIT("hello"), PROVEN_LIT("HELLO"));
    show("replace_first", &s);

    /* 없는 것을 바꾸라고 하면 아무 일도 하지 않고 성공한다 */
    e = proven_u8str_replace_first(&s, 0, PROVEN_LIT("zzz"), PROVEN_LIT("!"));
    proven_println("replacing something absent -> err={} (left alone, and it succeeds)",
                   PROVEN_ARG((int)e));

    /* ── ⑤ 구간 지우기 ───────────────────────────────────────── */
    e = proven_u8str_remove(&s, 0, 6);
    show("remove(0,6)", &s);

    /* ── ⑥ 되쓰기 — 버퍼는 그대로 두고 내용만 비운다 ──────────── */
    e = proven_u8str_reset(&s);
    show("after reset", &s);

    e = proven_u8str_append(&s, PROVEN_LIT("written again"));
    show("after refilling", &s);
    proven_println("(reset is for rewriting without reallocating - for code that rebuilds every frame)");

    /* ── ⑦ 미리 잡아 두기 ────────────────────────────────────── */
    e = proven_u8str_reserve(alloc, &s, 256);
    proven_println("reserve(256)      -> err={} (worth especially much on an arena)",
                   PROVEN_ARG((int)e));

    /* ── ⑧ 조회 연산들 ───────────────────────────────────────── */
    proven_u8str_view_t v = proven_u8str_as_view(&s);
    proven_println("starts_with(\"written\")={} ends_with(\"again\")={} find(\"ten\")={}",
                   PROVEN_ARG((bool)proven_u8str_view_starts_with(v, PROVEN_LIT("written"))),
                   PROVEN_ARG((bool)proven_u8str_view_ends_with(v, PROVEN_LIT("again"))),
                   PROVEN_ARG(proven_u8str_view_find(v, 0, PROVEN_LIT("ten"))));

    proven_size_t nf = proven_u8str_view_find(v, 0, PROVEN_LIT("absent"));
    proven_println("when it is not found: {} (PROVEN_INDEX_NOT_FOUND)",
                   PROVEN_ARG((bool)(nf == PROVEN_INDEX_NOT_FOUND)));

    proven_u8str_destroy(alloc, &s);
    return 0;
}
