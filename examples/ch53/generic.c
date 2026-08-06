#include <stdio.h>

/* C11 _Generic: 표현식의 *타입*에 따라 컴파일 시간에 하나를 고른다.
   실행 시간 비용이 0인 것이 핵심이다. */
#define TYPE_NAME(x) _Generic((x),      \
    _Bool:        "bool",               \
    char:         "char",               \
    int:          "int",                \
    unsigned:     "unsigned",           \
    long:         "long",               \
    double:       "double",             \
    float:        "float",              \
    const char *: "string",             \
    char *:       "string",             \
    default:      "other")

/* 값과 타입 꼬리표를 한 꾸러미로 묶는다 — proven 의 PROVEN_ARG 와 같은 착상 */
enum arg_kind { A_INT, A_DOUBLE, A_STR };

struct arg {
    enum arg_kind kind;
    union { long i; double d; const char *s; } v;
};

static struct arg arg_from_int(long v)        { return (struct arg){ A_INT,    { .i = v } }; }
static struct arg arg_from_double(double v)   { return (struct arg){ A_DOUBLE, { .d = v } }; }
static struct arg arg_from_str(const char *v) { return (struct arg){ A_STR,    { .s = v } }; }

#define ARG(x) _Generic((x),            \
    int:          arg_from_int,         \
    long:         arg_from_int,         \
    double:       arg_from_double,      \
    float:        arg_from_double,      \
    const char *: arg_from_str,         \
    char *:       arg_from_str)(x)

/* 인자의 타입을 함수가 *알고 받는다* — 서식 문자열을 믿지 않는다 */
static void print_args(const struct arg *a, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        switch (a[i].kind) {
            case A_INT:    printf("  [%zu] int    %ld\n", i, a[i].v.i); break;
            case A_DOUBLE: printf("  [%zu] double %.3f\n", i, a[i].v.d); break;
            case A_STR:    printf("  [%zu] string %s\n", i, a[i].v.s); break;
        }
    }
}

/* 배열 리터럴로 개수까지 함께 넘긴다 — 가변 인자가 아니다 */
#define PRINT(...) do {                                       \
    struct arg _args[] = { __VA_ARGS__ };                     \
    print_args(_args, sizeof _args / sizeof _args[0]);        \
} while (0)

int main(void)
{
    int         n = 42;
    double      x = 3.14159;
    const char *s = "hello";
    char        c = 'A';
    bool        b = true;

    printf("TYPE_NAME: %s %s %s %s %s\n",
           TYPE_NAME(n), TYPE_NAME(x), TYPE_NAME(s), TYPE_NAME(c), TYPE_NAME(b));

    printf("typed arguments:\n");
    PRINT(ARG(n), ARG(x), ARG(s));
    return 0;
}
