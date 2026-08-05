#include <proven.h>
#include <stdio.h>

int main(void)
{
    /* 화면이 아니라 문자열로 형식화한다 — 버퍼는 스택에서 빌린다 */
    proven_byte_t buf[64];
    proven_u8str_t out = proven_u8str_borrow(buf, sizeof buf);

    int      port = 8080;
    double   load = 0.4237;
    const char *host = "example.org";

    proven_fmt_result_t r = proven_u8str_append_fmt(&out, "{}:{} load={:.2}",
                                                   PROVEN_ARG(host), PROVEN_ARG(port),
                                                   PROVEN_ARG(load));
    if (proven_is_ok(r.err)) {
        proven_u8str_view_t v = proven_u8str_as_view(&out);
        printf("formatted : %.*s\n", (int)v.size, (const char *)v.ptr);
    }

    /* 그릇이 모자라면? 자르지 않고 거부한다 */
    proven_byte_t small_buf[8];
    proven_u8str_t small = proven_u8str_borrow(small_buf, sizeof small_buf);
    proven_fmt_result_t r2 = proven_u8str_append_fmt(&small, "{}:{}",
                                                    PROVEN_ARG(host), PROVEN_ARG(port));
    printf("into 8 bytes: %s (err=%d)\n",
           proven_is_ok(r2.err) ? "ok" : "refused", (int)r2.err);

    /* 정렬과 자릿수 — 46장의 폭/정밀도에 해당한다 */
    proven_u8str_t line = proven_u8str_borrow(buf, sizeof buf);
    (void)proven_u8str_reset(&line);
    proven_fmt_result_t r3 = proven_u8str_append_fmt(&line, "|{:>10}|{:<10}|{:.3}|",
                                                    PROVEN_ARG(host), PROVEN_ARG(host),
                                                    PROVEN_ARG(load));
    if (proven_is_ok(r3.err)) {
        proven_u8str_view_t v = proven_u8str_as_view(&line);
        printf("aligned   : %.*s\n", (int)v.size, (const char *)v.ptr);
    }
    return 0;
}
