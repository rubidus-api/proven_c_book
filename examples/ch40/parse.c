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
    if (!text) { r.why = "입력이 없다"; return r; }

    errno = 0;
    char *end = nullptr;
    long long v = strtoll(text, &end, 10);

    if (end == text)                 { r.why = "수로 시작하지 않는다";  return r; }
    if (errno == ERANGE)             { r.why = "이 타입에 담기지 않는다"; r.rest = end; return r; }

    r.ok = true; r.value = v; r.rest = end; r.why = "";
    return r;
}

static void show(const char *text)
{
    struct parse_i64 r = parse_int(text);
    if (r.ok)
        printf("  %-24s → 성공: %lld, 남은 입력: \"%s\"\n",
               text, r.value, r.rest);
    else
        printf("  %-24s → 실패: %s\n", text, r.why);
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
    puts("[① 실패를 값으로 돌려준다 — 성공 여부와 값을 갈라서]");
    show("42");
    show("  42 그리고 나머지");
    show("마흔둘");
    show("999999999999999999999999");

    puts("\n[② 어디까지 읽었는지 알려준다]");
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
    printf("  \"%s\" 의 합 = %lld  ← rest 가 있어 이어 읽을 수 있다\n", csv, sum);

    puts("\n[③ 잘림을 성공으로 치지 않는다]");
    char small[8];
    printf("  \"hello\" → %s\n",
           copy_line(small, sizeof small, "hello") ? "성공" : "실패(잘림)");
    printf("  \"hello, world\" → %s\n",
           copy_line(small, sizeof small, "hello, world") ? "성공" : "실패(잘림)");

    puts("\n[④ 실패하면 출력 인자를 건드리지 않는다]");
    long long keep = -1;
    struct parse_i64 bad = parse_int("없는 수");
    if (bad.ok) keep = bad.value;
    printf("  실패한 뒤 원래 값이 그대로인가: %s (keep = %lld)\n",
           keep == -1 ? "예" : "아니오", keep);

    puts("\n[⑤ 확인을 잊으면 컴파일러가 말하게 한다]");
    puts("  parse_int 와 copy_line 에는 [[nodiscard]] 가 붙어 있다.");
    puts("  반환값을 버리면 경고가 난다 — 실패 확인을 잊는 사고를 막는다.");
    return 0;
}
