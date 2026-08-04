#include <proven.h>
#include <stdio.h>

/* 한 줄에서 "이름 나이" 꼴을 해석한다 — 실패는 값으로 드러난다. */
static void parse_line(const char *line)
{
    proven_scan_t sc = proven_scan_init(proven_u8str_view_from_cstr(line));

    proven_result_u8str_view_t name = proven_scan_str(&sc);
    if (!proven_is_ok(name.err)) {
        printf("[%s] -> 이름을 찾지 못했다\n", line);
        return;
    }

    proven_result_i64_t age = proven_scan_i64(&sc);
    if (!proven_is_ok(age.err)) {
        printf("[%s] -> 나이가 수가 아니다\n", line);
        return;
    }

    printf("[%s] -> 이름 %.*s, 나이 %lld\n", line,
           (int)name.val.size, (const char *)name.val.ptr, (long long)age.val);
}

int main(void)
{
    parse_line("가영 33");
    parse_line("나준 서른");
    parse_line("   ");
    return 0;
}
