/* Uses C23 ckd_add from <stdckdint.h> to catch sum overflow and leave *out untouched on failure. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdckdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool sum_checked(const char *line, int64_t *out, const char **why)
{
    int64_t acc = 0;
    const char *p = line;
    for (;;) {
        errno = 0;
        char *end = nullptr;
        long long v = strtoll(p, &end, 10);
        if (end == p || errno == ERANGE) {
            *why = "bad integer or out of range";
            return false;
        }
        if (ckd_add(&acc, acc, (int64_t)v)) {
            *why = "sum overflow";
            return false;
        }
        while (*end == ' ' || *end == '\t') end++;
        if (*end == ',') { p = end + 1; continue; }
        if (*end == '\0' || *end == '\n') break;
        *why = "unexpected character";
        return false;
    }
    *out = acc;
    return true;
}

int main(void)
{
    /* First show why an unsigned post-check (sum < prev) misfires when negative numbers appear */
    uint64_t a = 10;
    uint64_t b = (uint64_t)(int64_t)-3;
    uint64_t wrapped = a + b;
    printf("[naive post-check] 10 + (-3) as uint64 = %" PRIu64 " (< 10 => false overflow: %s)\n",
           wrapped, wrapped < a ? "yes" : "no");

    const char *cases[] = {
        "10,-3",
        "9223372036854775807,1",
        "-9223372036854775808,-1",
        "9223372036854775807,-1,1",
        "5000000000000000000,5000000000000000000",
        "-5000000000000000000,-5000000000000000000",
    };

    puts("\n[ckd_add boundary checks]");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int64_t out = -999;
        const char *why = "ok";
        bool ok = sum_checked(cases[i], &out, &why);
        if (ok) {
            printf("  %-44s -> ok: %" PRId64 "\n", cases[i], out);
        } else {
            printf("  %-44s -> err: %s (out preserved: %" PRId64 ")\n",
                   cases[i], why, out);
        }
    }
    return 0;
}
