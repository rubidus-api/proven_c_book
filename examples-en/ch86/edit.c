/* The operations that change a string — inserting, removing, replacing and
   rewriting. Every one of them "does it if it fits, and if it does not, refuses
   and leaves the original as it was". */
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

    /* ── (1) made straight from a view ───────────────────────────── */
    proven_result_u8str_t r =
        proven_u8str_create_from_view(alloc, PROVEN_LIT("hello world"));
    if (!proven_is_ok(r.err)) return 1;
    proven_u8str_t s = r.value;
    show("just made", &s);

    /* ── (2) inserted in the middle ──────────────────────────────── */
    proven_err_t e = proven_u8str_insert_grow(alloc, &s, 5, PROVEN_LIT(","));
    show("insert(5, \",\")", &s);

    /* ── (3) a range replaced — the lengths need not match ───────── */
    e = proven_u8str_replace_at_grow(alloc, &s, 7, 5, PROVEN_LIT("proven C"));
    show("replace_at", &s);

    /* ── (4) the first occurrence replaced ───────────────────────── */
    e = proven_u8str_replace_first(&s, 0, PROVEN_LIT("hello"), PROVEN_LIT("HELLO"));
    show("replace_first", &s);

    /* asked to replace something that is not there, it does nothing and succeeds */
    e = proven_u8str_replace_first(&s, 0, PROVEN_LIT("zzz"), PROVEN_LIT("!"));
    proven_println("replacing what is absent -> err={} (left as it was, and success)",
                   PROVEN_ARG((int)e));

    /* ── (5) a range removed ─────────────────────────────────────── */
    e = proven_u8str_remove(&s, 0, 6);
    show("remove(0,6)", &s);

    /* ── (6) rewriting — the buffer stays, only the content is emptied ─ */
    e = proven_u8str_reset(&s);
    show("after reset", &s);

    e = proven_u8str_append(&s, PROVEN_LIT("written again"));
    show("after refilling", &s);
    proven_println("(reset is for rewriting without reallocating — for code that rebuilds every frame)");

    /* ── (7) reserved in advance ─────────────────────────────────── */
    e = proven_u8str_reserve(alloc, &s, 256);
    proven_println("reserve(256)      -> err={} (worth most on an arena)",
                   PROVEN_ARG((int)e));

    /* ── (8) the query operations ────────────────────────────────── */
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
