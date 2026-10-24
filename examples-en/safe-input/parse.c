/* Five disciplines for handling a failed parse — in standard C alone. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A result bundle that returns failure as a *value*.
   Success and value are kept apart, and how far it read comes back too. */
struct parse_i64 {
    bool        ok;
    long long   value;
    const char *rest;      /* where parsing stopped — to read on, or to point at */
    const char *why;       /* why it failed, in words a person reads */
};

/* [[nodiscard]] is C23 for "warn if this return value is thrown away".
   The compiler catches the mistake of forgetting to check. */
[[nodiscard]]
static struct parse_i64 parse_int(const char *text)
{
    struct parse_i64 r = { .ok = false, .value = 0, .rest = text, .why = "" };
    if (!text) { r.why = "no input"; return r; }

    errno = 0;
    char *end = nullptr;
    long long v = strtoll(text, &end, 10);

    if (end == text)                 { r.why = "does not start with a number"; return r; }
    if (errno == ERANGE)             { r.why = "does not fit this type"; r.rest = end; return r; }

    r.ok = true; r.value = v; r.rest = end; r.why = "";
    return r;
}

static void show(const char *text)
{
    struct parse_i64 r = parse_int(text);
    if (r.ok)
        printf("  %-24s -> ok: %lld, rest: \"%s\"\n",
               text, r.value, r.rest);
    else
        printf("  %-24s -> failed: %s\n", text, r.why);
}

/* A copy that treats truncation as an error — no "truncated but fine" */
[[nodiscard]]
static bool copy_line(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n + 1 > cap) return false;          /* if it would be cut, fail outright */
    memcpy(dst, src, n + 1);
    return true;
}

int main(void)
{
    puts("[1] failure as a value — success and value kept apart");
    show("42");
    show("  42 and the rest");
    show("forty-two");
    show("999999999999999999999999");

    puts("\n[2] it says how far it read");
    const char *csv = "10,20,30";
    long long sum = 0;
    const char *p = csv;
    for (;;) {
        struct parse_i64 r = parse_int(p);
        if (!r.ok) break;
        sum += r.value;
        p = r.rest;
        if (*p == ',') p++;
        else break;
    }
    printf("  the sum of \"%s\" = %lld  <- rest lets you carry on\n", csv, sum);

    puts("\n[3] truncation does not count as success");
    char small[8];
    printf("  \"hello\" -> %s\n",
           copy_line(small, sizeof small, "hello") ? "ok" : "failed (would truncate)");
    printf("  \"hello, world\" -> %s\n",
           copy_line(small, sizeof small, "hello, world") ? "ok" : "failed (would truncate)");

    puts("\n[4] on failure it touches no output");
    long long keep = -1;
    struct parse_i64 bad = parse_int("not a number");
    if (bad.ok) keep = bad.value;
    printf("  is the original value untouched after failure: %s (keep = %lld)\n",
           keep == -1 ? "yes" : "no", keep);

    puts("\n[5] let the compiler speak when a check is forgotten");
    puts("  parse_int and copy_line carry [[nodiscard]].");
    puts("  Throw the return value away and a warning appears.");
    return 0;
}
