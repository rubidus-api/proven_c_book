#include <proven.h>
#include <stdio.h>

void try_parse(const char *text)
{
    proven_scan_t sc = proven_scan_init(proven_u8str_view_from_cstr(text));
    proven_result_i64_t r = proven_scan_i64(&sc);

    if (proven_is_ok(r.err)) {
        printf("\"%s\" -> 성공: %lld\n", text, (long long)r.val);
    } else {
        printf("\"%s\" -> 실패 (수가 아니다)\n", text);
    }
}

int main(void)
{
    try_parse("  42 뒤에 다른 글");
    try_parse("사십이");
    return 0;
}
