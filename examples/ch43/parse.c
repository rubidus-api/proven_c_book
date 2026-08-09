/* 입력 해석의 실패를 다루는 다섯 규율 — 표준 C 만으로. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 실패를 '값'으로 돌려주는 결과 꾸러미.
   성공 여부와 값을 갈라 담고, 어디까지 읽었는지도 함께 돌려준다. */
struct parse_i64 {
    bool        ok;
    long long   value;
    const char *rest;      /* 해석이 멈춘 자리 — 이어 읽거나 오류를 가리킬 때 */
    const char *why;       /* 실패한 이유(사람이 읽는 말) */
};

/* [[nodiscard]] 는 "이 반환값을 버리면 경고하라"는 C23 의 표기다.
   실패 확인을 잊는 실수를 컴파일러가 잡아 준다. */
[[nodiscard]]
static struct parse_i64 parse_int(const char *text)
{
    struct parse_i64 r = { .ok = false, .value = 0, .rest = text, .why = "" };
    if (!text) { r.why = "empty input"; return r; }

    errno = 0;
    char *end = nullptr;
    long long v = strtoll(text, &end, 10);

    if (end == text)                 { r.why = "does not start with a number";  return r; }
    if (errno == ERANGE)             { r.why = "does not fit in this type"; r.rest = end; return r; }

    r.ok = true; r.value = v; r.rest = end; r.why = "";
    return r;
}

static void show(const char *text)
{
    struct parse_i64 r = parse_int(text);
    if (r.ok)
        printf("  %-24s -> ok: %lld, input left: \"%s\"\n",
               text, r.value, r.rest);
    else
        printf("  %-24s -> failed: %s\n", text, r.why);
}

/* 잘림을 오류로 다루는 복사 — "잘렸지만 성공"을 남기지 않는다 */
[[nodiscard]]
static bool copy_line(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n + 1 > cap) return false;          /* 잘릴 상황이면 아예 실패로 */
    memcpy(dst, src, n + 1);
    return true;
}

int main(void)
{
    puts("[1: return the failure as a value - success and value kept apart]");
    show("42");
    show("  42 and then some");
    show("forty-two");
    show("999999999999999999999999");

    puts("\n[2: say how far you read]");
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
    printf("  sum of \"%s\" = %lld  <- rest lets you keep reading\n", csv, sum);

    puts("\n[3: truncation does not count as success]");
    char small[8];
    printf("  \"hello\" → %s\n",
           copy_line(small, sizeof small, "hello") ? "ok" : "failed (truncated)");
    printf("  \"hello, world\" → %s\n",
           copy_line(small, sizeof small, "hello, world") ? "ok" : "failed (truncated)");

    puts("\n[4: on failure, leave the output argument alone]");
    long long keep = -1;
    struct parse_i64 bad = parse_int("not a number");
    if (bad.ok) keep = bad.value;
    printf("  is the original value still there after the failure: %s (keep = %lld)\n",
           keep == -1 ? "yes" : "no", keep);

    puts("\n[5: let the compiler speak when you forget to check]");
    puts("  parse_int and copy_line are marked [[nodiscard]].");
    puts("  dropping the return value warns - that stops you forgetting to check for failure.");
    return 0;
}
