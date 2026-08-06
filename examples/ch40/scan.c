#include <proven.h>
#include <stdio.h>

void try_parse(const char *text)
{
    proven_scan_t sc = proven_scan_init(proven_u8str_view_from_cstr(text));
    proven_result_i64_t r = proven_scan_i64(&sc);

    if (proven_is_ok(r.err)) {
        printf("\"%s\" -> ok: %lld\n", text, (long long)r.val);
    } else {
        printf("\"%s\" -> failed (not a number)\n", text);
    }
}

int main(void)
{
    try_parse("  42 and some text");
    try_parse("forty-two");
    return 0;
}
