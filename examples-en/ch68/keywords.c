/* What was promoted to a keyword in C23 — and what changed */
#include <stdio.h>

/* before the promotion these lines demanded <stdbool.h>, <stdalign.h>, <assert.h> */
static_assert(sizeof(int) >= 4, "this code assumes an int of 32 bits or more");

/* nullptr has a type of its own (nullptr_t) — it is neither 0 nor (void *)0 */
static void take_ptr(int *p) { printf("  pointer: %s\n", p ? "has a value" : "null"); }

/* the place where the difference between NULL and nullptr shows in variadic arguments */
static void print_all(const char *first, ...)
{
    printf("  %s ...\n", first);
}

int main(void)
{
    bool ready = true;                 /* a real keyword, neither _Bool nor a macro */
    printf("sizeof bool = %zu, true = %d, false = %d\n", sizeof(bool), true, false);
    printf("ready = %d, !ready = %d\n", ready, !ready);

    /* bool narrows every non-zero value to 1 — a property an int cannot imitate */
    bool  b = 42;
    int   i = 42;
    printf("bool b = 42 -> %d,  int i = 42 -> %d\n", b, i);

    int   x  = 7;
    int  *p  = &x;
    int  *np = nullptr;
    take_ptr(p);
    take_ptr(np);
    printf("nullptr compared with nullptr: %d,  with a pointer: %d\n", nullptr == nullptr, p == nullptr);

    /* typeof: it writes down a nameless type as it is */
    typeof(x) y = x * 2;
    printf("typeof(x) y = %d\n", y);

    /* constexpr: a real constant — usable as an array size and as a switch label */
    constexpr int LANES = 4;
    int lane[LANES];
    printf("constexpr LANES = %d, elements in the array = %zu\n", LANES, sizeof lane / sizeof lane[0]);

    print_all("pass nullptr in variadic arguments", nullptr);
    return 0;
}
