/* 에러는 값이다 — 그 값이 실제로 어떤 것들인지 눈으로 본다. */
#include <proven.h>

static const char *name_of(proven_err_t e)
{
    switch (e) {
    case PROVEN_OK:                   return "PROVEN_OK";
    case PROVEN_ERR_NOMEM:            return "NOMEM (기억이 모자람)";
    case PROVEN_ERR_OUT_OF_BOUNDS:    return "OUT_OF_BOUNDS (그릇 밖)";
    case PROVEN_ERR_INVALID_ENCODING: return "INVALID_ENCODING (인코딩이 깨짐)";
    case PROVEN_ERR_INVALID_ARG:      return "INVALID_ARG (인자가 계약 밖)";
    case PROVEN_ERR_IO:               return "IO (바깥 세계의 실패)";
    case PROVEN_ERR_NOT_FOUND:        return "NOT_FOUND (없음)";
    case PROVEN_ERR_INVALID_STATE:    return "INVALID_STATE (지금 할 수 없음)";
    case PROVEN_ERR_NEED_MORE:        return "NEED_MORE (입력이 더 필요)";
    case PROVEN_ERR_OVERFLOW:         return "OVERFLOW (계산이 넘침)";
    case PROVEN_ERR_UNSUPPORTED:      return "UNSUPPORTED (이 환경엔 없음)";
    case PROVEN_ERR_AGAIN:            return "AGAIN (지금은 말고 다시)";
    case PROVEN_ERR_EOF:              return "EOF (끝에 닿음)";
    case PROVEN_ERR_BUSY:             return "BUSY (남이 쓰는 중)";
    case PROVEN_ERR_PERMISSION:       return "PERMISSION (권한 없음)";
    case PROVEN_ERR_INVALID_FORMAT:   return "INVALID_FORMAT (형식이 틀림)";
    }
    return "(알 수 없음)";
}

int main(void)
{
    proven_println("― 에러 코드 전수 ―");
    for (int i = PROVEN_OK; i <= PROVEN_ERR_INVALID_FORMAT; i++)
        proven_println("{:>2}  {}", PROVEN_ARG(i), PROVEN_ARG(name_of((proven_err_t)i)));

    proven_println("");
    proven_println("― 실제로 어떤 코드가 돌아오는가 ―");

    /* ① 그릇이 모자라다 → OUT_OF_BOUNDS */
    proven_byte_t small[8];
    proven_u8str_t s = proven_u8str_borrow(small, sizeof small);
    proven_err_t e1 = proven_u8str_append(&s, PROVEN_LIT("Hello, world"));
    proven_println("append 12 bytes into 8   -> {}", PROVEN_ARG(name_of(e1)));

    /* 실패 원자성: 거부된 뒤에도 원본은 손대지 않은 상태다 */
    proven_println("  after failure, len = {}",
                   PROVEN_ARG(proven_u8str_as_view(&s).size));

    /* ② 계약을 어긴 인자 → INVALID_ARG.
          쓸 수 없는 할당자(전부 0인 값)를 주면 만들기 전에 걸러낸다. */
    proven_allocator_t nothing = (proven_allocator_t){0};
    proven_result_u8str_t bad = proven_u8str_create(nothing, 16);
    proven_println("create with a null allocator -> {}", PROVEN_ARG(name_of(bad.err)));

    /* ③ 범위 밖 자르기 → OUT_OF_BOUNDS (있는 만큼 주지 않는다) */
    proven_u8str_view_t v = PROVEN_LIT("abcdefgh");
    proven_result_mem_view_t sl =
        proven_mem_view_slice_checked(proven_mem_view_from_u8(v), 6, 4);
    proven_println("slice 4 bytes from 6/8   -> {}", PROVEN_ARG(name_of(sl.err)));

    /* ④ 성공은 언제나 0이고, 확인은 언제나 같은 한 줄이다 */
    proven_err_t ok = proven_u8str_append(&s, PROVEN_LIT("hi"));
    proven_println("append 2 bytes into 8    -> {} (is_ok={})",
                   PROVEN_ARG(name_of(ok)), PROVEN_ARG(proven_is_ok(ok)));
    return 0;
}
