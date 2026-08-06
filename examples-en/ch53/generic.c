#include <stdio.h>

/* C11 _Generic: it picks one at compile time according to the *type* of the
   expression. That the run-time cost is zero is the point. */
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

/* bundling the value with a type tag — the same idea as proven's PROVEN_ARG */
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

/* the function *knows* the type of what it receives — it does not trust a format string */
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

/* an array literal carries the count along too — these are not variadic arguments */
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
