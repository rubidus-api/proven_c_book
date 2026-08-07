/* 함수 포인터를 인쇄하는 법 — %p 에 넘길 수 없는 값을 다루는 세 가지 길. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
static int mul(int a, int b) { return a * b; }

typedef int (*binop)(int, int);

/* ── 길 ①: 바이트로 떠서 16진수로 ────────────────────────────
   함수 포인터를 void* 로 캐스트하는 것은 표준 밖이다. memcpy 로 바이트를
   옮기는 것은 어디서나 계약 안이다 — 크기만 맞으면 된다. */
static void fmt_funcptr(char *out, size_t cap, binop f)
{
    unsigned char raw[sizeof f];
    memcpy(raw, &f, sizeof raw);

    size_t k = 0;
    k += (size_t)snprintf(out + k, cap - k, "0x");
    for (size_t i = sizeof raw; i-- > 0 && k + 2 < cap; )
        k += (size_t)snprintf(out + k, cap - k, "%02X", raw[i]);
}

/* ── 길 ②: 이름표를 함께 들고 다닌다 (실무의 정답) ───────────── */
struct named_op {
    const char *name;
    binop       fn;
};

static const struct named_op ops[] = {
    { "add", add }, { "sub", sub }, { "mul", mul },
};

static const char *name_of(binop f)
{
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++)
        if (ops[i].fn == f) return ops[i].name;      /* 함수 포인터 비교는 합법 */
    return "(모르는 함수)";
}

int main(void)
{
    printf("sizeof(함수 포인터) = %zu, sizeof(void *) = %zu\n\n",
           sizeof(binop), sizeof(void *));

    puts("[길 ①] 바이트로 떠서 인쇄 — 이식성은 있지만 사람에게는 수수께끼다");
    char buf[2 + 2 * sizeof(binop) + 1];
    fmt_funcptr(buf, sizeof buf, add);
    printf("  add 의 주소 문자열 길이: %zu글자 (값은 실행마다 달라 싣지 않는다)\n",
           strlen(buf));
    printf("  \"0x\" 로 시작하는가: %s\n", strncmp(buf, "0x", 2) == 0 ? "예" : "아니오");

    puts("\n[길 ②] 이름표로 인쇄 — 로그에 남길 것은 이쪽이다");
    binop chosen[] = { mul, add, sub };
    for (size_t i = 0; i < sizeof chosen / sizeof *chosen; i++)
        printf("  호출 %zu: %s(7, 3) = %d\n",
               i, name_of(chosen[i]), chosen[i](7, 3));

    puts("\n[함수 포인터끼리의 비교는 계약 안이다]");
    binop f = add, g = add, h = sub;
    printf("  같은 함수를 가리키면 같은가: %s\n", f == g ? "예" : "아니오");
    printf("  다른 함수면 다른가:         %s\n", f != h ? "예" : "아니오");
    puts("  그래서 '어느 함수인가'는 주소를 인쇄하지 않고도 알아낼 수 있다.");

    puts("\n[하면 안 되는 것]");
    puts("  printf(\"%p\", f);          ← 함수 포인터를 %p 에 넘기는 것: 계약 밖");
    puts("  printf(\"%p\", (void *)f);  ← 표준 밖(POSIX 에서는 통한다). -Wpedantic 이 잡는다");
    puts("  printf(\"%p\", (void *)&f); ← 컴파일은 되지만 *변수의 주소*다. 원하던 값이 아니다");
    return 0;
}
