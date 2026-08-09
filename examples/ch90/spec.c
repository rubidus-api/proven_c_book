/* 자리표시자 문법 전부 — 채움·정렬·폭·정밀도·꼴, 그리고 사용자 타입.
   {} 하나로 시작해 표 하나를 그리는 데까지 간다. */
#include <proven.h>

/* 라이브러리가 모르는 타입을 찍는 법: 그리는 함수를 하나 주면 된다 */
typedef struct { int num; int den; } frac_t;

static proven_err_t render_frac(proven_fmt_sink_t out, const void *obj)
{
    const frac_t *f = (const frac_t *)obj;
    proven_byte_t buf[32];
    proven_u8str_t s = proven_u8str_borrow(buf, sizeof buf);
    proven_fmt_result_t r = proven_u8str_append_fmt(&s, "{}/{}",
                                                    PROVEN_ARG(f->num),
                                                    PROVEN_ARG(f->den));
    if (!proven_is_ok(r.err)) return r.err;
    return proven_fmt_put(out, proven_u8str_as_view(&s));
}

int main(void)
{
    proven_byte_t buf[256];

    /* ── ① 정렬과 채움 ───────────────────────────────────────── */
    proven_println("|{:>8}|{:<8}|{:^8}|  (right, left, centre)",
                   PROVEN_ARG("ab"), PROVEN_ARG("ab"), PROVEN_ARG("ab"));
    proven_println("|{:*>8}|{:-<8}|{:.^8}|  (the fill character goes first)",
                   PROVEN_ARG("ab"), PROVEN_ARG("ab"), PROVEN_ARG("ab"));
    proven_println("|{:08}|{:+}|{:+}|      (zero fill, forced sign)",
                   PROVEN_ARG(42), PROVEN_ARG(42), PROVEN_ARG(-42));

    /* ── ② 진법과 대체 형식 ──────────────────────────────────── */
    proven_println("{:x} {:X} {:#x} {:o} {:b} {:#b}",
                   PROVEN_ARG(255), PROVEN_ARG(255), PROVEN_ARG(255),
                   PROVEN_ARG(8), PROVEN_ARG(5), PROVEN_ARG(5));

    /* ── ③ 실수 — 자릿수와 꼴 ────────────────────────────────── */
    proven_println("{} {:.2} {:.0} {:f} {:e} {:g}",
                   PROVEN_ARG(3.14159), PROVEN_ARG(3.14159), PROVEN_ARG(3.14159),
                   PROVEN_ARG(3.14159), PROVEN_ARG(3.14159), PROVEN_ARG(3.14159));
    proven_println("very large and very small: {} {}",
                   PROVEN_ARG(1e20), PROVEN_ARG(5e-7));

    /* ── ④ 중괄호 자체 ───────────────────────────────────────── */
    proven_println("braces are written {{ and }}");

    /* ── ⑤ 사용자 타입 ───────────────────────────────────────── */
    frac_t half = { .num = 1, .den = 2 };
    proven_arg_t a = proven_arg_custom(&half, render_frac);
    proven_println("a user type: {} (it honours the width too: |{:>8}|)",
                   PROVEN_ARG(a), PROVEN_ARG(a));

    /* ── ⑥ 세 갈래의 형식화 — 거부 / 자름 / 성장 ─────────────── */
    proven_u8str_t small = proven_u8str_borrow(buf, 8);   /* 내용 7바이트까지 */

    proven_fmt_result_t r1 = proven_u8str_append_fmt(&small, "{}", PROVEN_ARG("abcdefgh"));
    proven_println("append_fmt        err={} written={} required={}",
                   PROVEN_ARG((int)r1.err), PROVEN_ARG(r1.written), PROVEN_ARG(r1.required));

    proven_fmt_result_t r2 = proven_u8str_append_fmt_trunc(&small, "{}", PROVEN_ARG("abcdefghij"));
    proven_println("append_fmt_trunc  err={} written={} required={} contents=\"{}\"",
                   PROVEN_ARG((int)r2.err), PROVEN_ARG(r2.written), PROVEN_ARG(r2.required),
                   PROVEN_ARG(proven_u8str_as_view(&small)));

    proven_allocator_t alloc = proven_heap_allocator();
    proven_result_u8str_t made = proven_u8str_create(alloc, 4);
    if (proven_is_ok(made.err)) {
        proven_u8str_t g = made.value;
        proven_fmt_result_t r3 = proven_u8str_append_fmt_grow(alloc, &g, "{} {} {}",
                                                              PROVEN_ARG("it grows"),
                                                              PROVEN_ARG(2026),
                                                              PROVEN_ARG(true));
        proven_println("append_fmt_grow   err={} contents=\"{}\"",
                       PROVEN_ARG((int)r3.err), PROVEN_ARG(proven_u8str_as_view(&g)));
        proven_u8str_destroy(alloc, &g);
    }

    /* ── ⑦ 표 그리기 — 폭 지정의 실제 쓸모 ───────────────────── */
    proven_println("");
    proven_println("{:<10}{:>6}{:>9}", PROVEN_ARG("name"), PROVEN_ARG("count"), PROVEN_ARG("ratio"));
    proven_println("{:-<25}", PROVEN_ARG(""));
    const char *names[] = { "alpha", "beta", "gamma" };
    int         counts[] = { 7, 128, 3 };
    for (int i = 0; i < 3; i++)
        proven_println("{:<10}{:>6}{:>9.2}", PROVEN_ARG(names[i]),
                       PROVEN_ARG(counts[i]), PROVEN_ARG(counts[i] / 138.0 * 100));
    return 0;
}
