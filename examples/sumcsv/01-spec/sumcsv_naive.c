/* 44장에서 가져온 파서를 곧바로 합산 루프에 끼운 첫 초안 ---
   정책이 없으면 잘못된 입력에도 합계와 종료 상태 0 이 나온다. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct parse_i64 {
    bool        ok;
    long long   value;
    const char *rest;
};

static struct parse_i64 parse_int(const char *text)
{
    struct parse_i64 r = { .ok = false, .value = 0, .rest = text };
    if (!text) return r;

    errno = 0;
    char *end = nullptr;
    long long v = strtoll(text, &end, 10);
    if (end == text || errno == ERANGE) return r;

    r.ok = true;
    r.value = v;
    r.rest = end;
    return r;
}

int main(void)
{
    char line[256];
    if (!fgets(line, sizeof line, stdin)) {
        printf("0\n");
        return 0;
    }

    long long sum = 0;
    const char *p = line;
    for (;;) {
        struct parse_i64 r = parse_int(p);
        if (!r.ok) break;
        sum += r.value;
        p = r.rest;
        if (*p == ',') p++;
        else break;
    }
    printf("%lld\n", sum);
    return 0;
}
