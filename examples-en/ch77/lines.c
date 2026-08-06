#include <proven.h>
#include <stdio.h>

/* A line of the shape "name age" interpreted — failure surfaces as a value. */
static void parse_line(const char *line)
{
    proven_scan_t sc = proven_scan_init(proven_u8str_view_from_cstr(line));

    proven_result_u8str_view_t name = proven_scan_str(&sc);
    if (!proven_is_ok(name.err)) {
        printf("[%s] -> no name found\n", line);
        return;
    }

    proven_result_i64_t age = proven_scan_i64(&sc);
    if (!proven_is_ok(age.err)) {
        printf("[%s] -> age is not a number\n", line);
        return;
    }

    printf("[%s] -> name %.*s, age %lld\n", line,
           (int)name.val.size, (const char *)name.val.ptr, (long long)age.val);
}

int main(void)
{
    parse_line("alice 33");
    parse_line("bob thirty");
    parse_line("   ");
    return 0;
}
