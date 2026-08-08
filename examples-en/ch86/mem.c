/* The vocabulary of memory — ownership (mem_t), a reading view, a writing view,
   and alignment. What the structs look like and what they promise, seen with
   the eyes. */
#include <proven.h>

/* a helper for seeing a byte view as text — both are {pointer, length}, so it carries straight over */
static proven_u8str_view_t as_text(proven_mem_view_t v)
{
    return (proven_u8str_view_t){ .ptr = v.ptr, .size = v.size };
}

static void show(const char *label, proven_mem_view_t v)
{
    proven_println("{:<14} size={} content=\"{}\"",
                   PROVEN_ARG(label), PROVEN_ARG(v.size), PROVEN_ARG(as_text(v)));
}

int main(void)
{
    /* ── (1) the three words ────────────────────────────────────
       proven_mem_t      { proven_byte_t *ptr; size };   an owned lump
       proven_mem_view_t { const byte *ptr; size };      a borrowed reading window
       proven_mem_mut_t  { byte *ptr; size };            a borrowed writing window
       All three tie "the pointer and the length" together so they cannot part. */
    proven_byte_t storage[16] = "abcdefghijklmno";

    proven_mem_mut_t  mut  = { .ptr = storage, .size = 15 };
    proven_mem_view_t view = { .ptr = storage, .size = 15 };

    /* through a writing view it can be changed; through a reading view it cannot (a compile error) */
    mut.ptr[0] = 'A';

    proven_println("sizeof(mem_view_t)={} (pointer + length)",
                   PROVEN_ARG(sizeof(proven_mem_view_t)));
    show("whole view", view);

    /* ── (2) slicing — the checked and the unchecked ─────────────── */
    proven_result_mem_view_t ok = proven_mem_view_slice_checked(view, 3, 5);
    if (proven_is_ok(ok.err)) show("slice(3,5)", ok.value);

    proven_result_mem_view_t bad = proven_mem_view_slice_checked(view, 12, 9);
    proven_println("slice(12,9)     -> err={} (refused rather than clamped)",
                   PROVEN_ARG((int)bad.err));

    /* In a hot loop where the bounds are already checked, the unchecked one is
       used. The rule "a dangerous choice only under a visible name" is here. */
    proven_mem_view_t fast = proven_mem_view_slice_unchecked(view, 0, 3);
    show("unchecked(0,3)", fast);

    /* ── (3) copying and moving that keep to the bounds ──────────── */
    proven_byte_t dst[8] = {0};
    proven_err_t e1 = proven_mem_copy(dst, sizeof dst,
                                      proven_mem_view_slice_unchecked(view, 0, 5));
    proven_println("copy 5 into 8   -> err={}", PROVEN_ARG((int)e1));

    proven_err_t e2 = proven_mem_copy(dst, sizeof dst, view);   /* 15 > 8 */
    proven_println("copy 15 into 8  -> err={} (if it does not fit, nothing is written)",
                   PROVEN_ARG((int)e2));

    /* overlapping regions go through move. Chapter 64's memcpy/memmove split holds here too */
    proven_err_t e3 = proven_mem_move(storage + 2, 13,
                                      proven_mem_view_slice_unchecked(view, 0, 5));
    show("after move", (proven_mem_view_t){ .ptr = storage, .size = 7 });
    proven_println("move overlap    -> err={}", PROVEN_ARG((int)e3));

    /* ── (4) is this pointer inside that lump? ───────────────────── */
    proven_size_t off = 0;
    bool inside = proven_range_contains_ptr(storage, sizeof storage,
                                            storage + 4, 2, &off);
    proven_println("is storage+4 inside? {} (offset {})",
                   PROVEN_ARG(inside), PROVEN_ARG(off));

    /* ── (5) alignment ───────────────────────────────────────────── */
    proven_println("align_up(13,8)={}  align_up(16,8)={}  MAX_ALIGN={}",
                   PROVEN_ARG(proven_mem_align_up(13, 8)),
                   PROVEN_ARG(proven_mem_align_up(16, 8)),
                   PROVEN_ARG((proven_size_t)PROVEN_MAX_ALIGN));
    proven_println("is_pow2(8)={} is_pow2(12)={} (an alignment must be a power of two)",
                   PROVEN_ARG(proven_is_pow2(8)), PROVEN_ARG(proven_is_pow2(12)));
    return 0;
}
