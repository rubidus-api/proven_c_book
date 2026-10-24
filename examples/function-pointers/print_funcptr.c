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
    return "(unknown function)";
}

int main(void)
{
    printf("sizeof(function pointer) = %zu, sizeof(void *) = %zu\n\n",
           sizeof(binop), sizeof(void *));

    puts("[road 1] print the bytes - portable, but a riddle to a human");
    char buf[2 + 2 * sizeof(binop) + 1];
    fmt_funcptr(buf, sizeof buf, add);
    printf("  length of the address string for add: %zu characters (the value differs per run, so it is not shown)\n",
           strlen(buf));
    printf("  does it start with \"0x\": %s\n", strncmp(buf, "0x", 2) == 0 ? "yes" : "no");

    puts("\n[road 2] print the label - this is what belongs in a log");
    binop chosen[] = { mul, add, sub };
    for (size_t i = 0; i < sizeof chosen / sizeof *chosen; i++)
        printf("  call %zu: %s(7, 3) = %d\n",
               i, name_of(chosen[i]), chosen[i](7, 3));

    puts("\n[comparing function pointers is inside the contract]");
    binop f = add, g = add, h = sub;
    printf("  same function, so equal: %s\n", f == g ? "yes" : "no");
    printf("  different function, so unequal: %s\n", f != h ? "yes" : "no");
    puts("  so 'which function is this' can be answered without printing an address.");

    puts("\n[what not to do]");
    puts("  printf(\"%p\", f);          <- passing a function pointer to %p: outside the contract");
    puts("  printf(\"%p\", (void *)f);  <- outside ISO C (POSIX allows it). -Wpedantic catches it");
    puts("  printf(\"%p\", (void *)&f); <- compiles, but that is *the address of the variable*, not what you wanted");
    return 0;
}
