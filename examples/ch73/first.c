/* 첫 실전 프로그램 — 객체를 만들고, 쓰고, 돌려주는 전 과정.
   이 파일 하나에 proven 프로그램의 뼈대가 다 들어 있다. */
#include <proven.h>

/* ① 기억은 어디서 오는가 — 호출자가 정한다(할당자 매개변수).
   ② 실패는 값으로 온다 — err 를 확인하기 전에는 value 를 보지 않는다.
   ③ 만든 것은 만들 때 준 할당자로 돌려준다. */
static proven_err_t build_line(proven_allocator_t alloc,
                               proven_u8str_view_t who,
                               int count,
                               proven_u8str_t *out)
{
    /* 용량 64바이트짜리 문자열을 alloc 에서 얻는다 */
    proven_result_u8str_t made = proven_u8str_create(alloc, 64);
    if (!proven_is_ok(made.err))
        return made.err;

    proven_u8str_t line = made.value;      /* 확인 뒤에야 꺼낸다 */

    /* 서식으로 이어 붙인다. 실패하면 원본은 손대지 않은 채 남는다.
       형식화는 err 와 함께 "쓴 바이트/필요한 바이트"까지 돌려준다 */
    proven_fmt_result_t r = proven_u8str_append_fmt(&line, "{} has {} message(s)",
                                                    PROVEN_ARG(who), PROVEN_ARG(count));
    if (!proven_is_ok(r.err)) {
        proven_u8str_destroy(alloc, &line); /* 실패 경로에서도 반납한다 */
        return r.err;
    }

    *out = line;                            /* 소유권이 호출자에게 넘어간다 */
    return PROVEN_OK;
}

int main(void)
{
    /* 힙 할당자 — 표준 malloc 을 라이브러리 인터페이스로 감싼 것 */
    proven_allocator_t alloc = proven_heap_allocator();

    proven_u8str_t line;
    proven_err_t e = build_line(alloc, PROVEN_LIT("alice"), 3, &line);
    if (!proven_is_ok(e)) {
        proven_println("build failed: {}", PROVEN_ARG((int)e));
        return 1;
    }

    /* 소유 문자열 → 빌린 뷰. 뷰는 원본이 살아 있는 동안만 유효하다 */
    proven_u8str_view_t v = proven_u8str_as_view(&line);
    proven_println("line   = {}", PROVEN_ARG(v));
    proven_println("length = {} bytes", PROVEN_ARG(v.size));

    /* NUL 종단이 필요한 옛 API 와 만나는 자리 (복사·할당 없음) */
    proven_println("as C string = {}", PROVEN_ARG(proven_u8str_as_cstr(&line)));

    proven_u8str_destroy(alloc, &line);     /* 만들 때 준 그 할당자로 */

    /* 파괴는 구조체를 0으로 비운다 — 되돌린 버퍼를 다시 가리키지 않도록.
       그래서 파괴 뒤의 길이는 0이고, 이 객체는 더 쓰지 않는 것이 계약이다. */
    proven_println("after destroy, length = {}",
                   PROVEN_ARG(proven_u8str_as_view(&line).size));
    return 0;
}
