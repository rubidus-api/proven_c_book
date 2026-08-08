/* 기억을 다루는 네 어휘 — 소유(mem_t), 읽기 뷰, 쓰기 뷰, 그리고 정렬.
   구조체가 어떻게 생겼고 무엇을 약속하는지 눈으로 확인한다. */
#include <proven.h>

/* 바이트 뷰를 글자로 보기 위한 도우미 — 둘 다 {포인터, 길이}라 그대로 옮긴다 */
static proven_u8str_view_t as_text(proven_mem_view_t v)
{
    return (proven_u8str_view_t){ .ptr = v.ptr, .size = v.size };
}

static void show(const char *label, proven_mem_view_t v)
{
    proven_println("{:<14} size={} 내용=\"{}\"",
                   PROVEN_ARG(label), PROVEN_ARG(v.size), PROVEN_ARG(as_text(v)));
}

int main(void)
{
    /* ── ① 세 가지 어휘 ──────────────────────────────────────────
       proven_mem_t      { proven_byte_t *ptr; size };   소유한 덩어리
       proven_mem_view_t { const byte *ptr; size };      빌린 읽기 창
       proven_mem_mut_t  { byte *ptr; size };            빌린 쓰기 창
       셋 다 "포인터와 길이"가 헤어지지 않게 묶은 것이다. */
    proven_byte_t storage[16] = "abcdefghijklmno";

    proven_mem_mut_t  mut  = { .ptr = storage, .size = 15 };
    proven_mem_view_t view = { .ptr = storage, .size = 15 };

    /* 쓰기 뷰로는 고칠 수 있고, 읽기 뷰로는 고칠 수 없다(컴파일 오류) */
    mut.ptr[0] = 'A';

    proven_println("sizeof(mem_view_t)={} (포인터 + 길이)",
                   PROVEN_ARG(sizeof(proven_mem_view_t)));
    show("전체 뷰", view);

    /* ── ② 자르기 — 검사판과 무검사판 ─────────────────────────── */
    proven_result_mem_view_t ok = proven_mem_view_slice_checked(view, 3, 5);
    if (proven_is_ok(ok.err)) show("slice(3,5)", ok.value);

    proven_result_mem_view_t bad = proven_mem_view_slice_checked(view, 12, 9);
    proven_println("slice(12,9)     -> err={} (자르지 않고 거부)",
                   PROVEN_ARG((int)bad.err));

    /* 경계를 이미 확인한 뜨거운 루프에서는 무검사판을 쓴다.
       "위험한 선택은 눈에 보이는 이름으로만" 이라는 규칙이 여기 있다. */
    proven_mem_view_t fast = proven_mem_view_slice_unchecked(view, 0, 3);
    show("unchecked(0,3)", fast);

    /* ── ③ 경계를 지키는 복사와 이동 ─────────────────────────── */
    proven_byte_t dst[8] = {0};
    proven_err_t e1 = proven_mem_copy(dst, sizeof dst,
                                      proven_mem_view_slice_unchecked(view, 0, 5));
    proven_println("copy 5 into 8   -> err={}", PROVEN_ARG((int)e1));

    proven_err_t e2 = proven_mem_copy(dst, sizeof dst, view);   /* 15 > 8 */
    proven_println("copy 15 into 8  -> err={} (넘치면 아예 안 쓴다)",
                   PROVEN_ARG((int)e2));

    /* 겹치는 영역은 move 로. 64장의 memcpy/memmove 구분이 여기서도 같다 */
    proven_err_t e3 = proven_mem_move(storage + 2, 13,
                                      proven_mem_view_slice_unchecked(view, 0, 5));
    show("move 후 원본", (proven_mem_view_t){ .ptr = storage, .size = 7 });
    proven_println("move overlap    -> err={}", PROVEN_ARG((int)e3));

    /* ── ④ 포인터가 그 덩어리 안에 있는가 ────────────────────── */
    proven_size_t off = 0;
    bool inside = proven_range_contains_ptr(storage, sizeof storage,
                                            storage + 4, 2, &off);
    proven_println("storage+4 는 안에 있는가? {} (offset {})",
                   PROVEN_ARG(inside), PROVEN_ARG(off));

    /* ── ⑤ 정렬 ──────────────────────────────────────────────── */
    proven_println("align_up(13,8)={}  align_up(16,8)={}  MAX_ALIGN={}",
                   PROVEN_ARG(proven_mem_align_up(13, 8)),
                   PROVEN_ARG(proven_mem_align_up(16, 8)),
                   PROVEN_ARG((proven_size_t)PROVEN_MAX_ALIGN));
    proven_println("is_pow2(8)={} is_pow2(12)={} (정렬은 2의 거듭제곱이어야)",
                   PROVEN_ARG(proven_is_pow2(8)), PROVEN_ARG(proven_is_pow2(12)));
    return 0;
}
