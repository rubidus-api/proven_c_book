/* C23's constexpr - how far a "real constant" goes. */
#include <stdio.h>

constexpr int table_size = 4;          /* file scope: static storage duration, internal linkage */
constexpr double half = 0.5;
constexpr long long big = 1LL << 40;

int table[table_size];                 /* an array size - and not a variable length array */
static_assert(sizeof table / sizeof table[0] == 4, "table_size is a constant");

constexpr int doubled = table_size * 2;  /* a constant built from a constant */

struct limits { int low, high; };
constexpr struct limits range = { 1, 9 };
static_assert(range.high == 9, "a member of a constexpr struct is a constant");

/* the preprocessor knows nothing of constexpr - the program prints that fact */
#if table_size == 4
static const char *preproc = "the preprocessor saw table_size == 4";
#else
static const char *preproc = "the preprocessor never saw it: the name became 0";
#endif

static const char *classify(int x)
{
    switch (x) {
    case table_size:  return "exactly the table size";   /* a case label */
    case doubled:     return "twice the table size";
    default:          return "something else";
    }
}

struct packed {
    unsigned flags : table_size;       /* the width of a bit-field */
};

int main(void)
{
    static int copy = table_size + 1;  /* static initialization */
    enum { same_again = table_size };  /* the value of an enumeration constant */

    printf("[constexpr is a constant expression]\n");
    printf("  array size    : %zu\n", sizeof table / sizeof table[0]);
    printf("  case label    : %s\n", classify(4));
    printf("  case label    : %s\n", classify(8));
    printf("  static init   : %d\n", copy);
    printf("  enum value    : %d\n", (int)same_again);
    printf("  bit-field     : %d bits\n", table_size);
    printf("  struct member : range.high = %d (checked with static_assert)\n",
           range.high);

    printf("\n[it has a type, unlike a macro]\n");
    printf("  half  = %g (double)\n", half);
    printf("  big   = %lld (long long)\n", big);
    printf("  const is implicit: %s\n",
           _Generic(&table_size, const int *: "yes, &table_size is const int *",
                                 int *: "no", default: "?"));

    printf("\n[a block-scope constexpr is an ordinary object with an address]\n");
    constexpr int local = 7;
    const int *p = &local;
    printf("  local = %d, read through a pointer = %d\n", local, *p);

    printf("\n[but the preprocessor runs before any of this]\n");
    printf("  %s\n", preproc);
    printf("  #if and #define live in a different world (chapter 57)\n");

    /* every line below is a compile error - the value must be exactly representable.
           constexpr unsigned int m = -1;      the value does not fit
           constexpr float f = 0.1;            0.1 as a double is not exact as a float
           constexpr int *q = &copy;           a pointer initializer must be null
           constexpr const char *s = "abc";    the same reason
           constexpr volatile int v = 1;       volatile, restrict and atomic are banned
           constexpr int no_init;              it must be a definition with an initializer */
    return 0;
}
