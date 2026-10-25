/* Overloading in C: _Generic picks a branch by the type of an expression,
   at compile time. The type it sees is not always the type you had in mind. */
#include <stdint.h>
#include <stdio.h>

#define type_name(x) _Generic((x),                          \
    char: "char", signed char: "signed char", unsigned char: "unsigned char", \
    short: "short", unsigned short: "unsigned short",        \
    int: "int", unsigned: "unsigned", long: "long", unsigned long: "unsigned long", \
    long long: "long long", unsigned long long: "unsigned long long", \
    float: "float", double: "double", long double: "long double", \
    char *: "char *", const char *: "const char *", int *: "int *", \
    default: "(something else)")

/* a "print" overloaded by type --- each branch must still compile for every call */
static void print_int(long long v)    { printf("%lld", v); }
static void print_uint(unsigned long long v) { printf("%llu", v); }
static void print_dbl(double v)       { printf("%g", v); }
static void print_str(const char *s)  { printf("\"%s\"", s); }
#define print(x) _Generic((x),                                          \
    int: print_int, long: print_int, long long: print_int,               \
    unsigned: print_uint, unsigned long: print_uint, unsigned long long: print_uint, \
    float: print_dbl, double: print_dbl,                                 \
    char *: print_str, const char *: print_str)(x)

int main(void)
{
    char c = 'a';
    uint8_t byte = 200;
    const int ci = 5;
    int arr[3] = { 0 };

    /* a character constant is an int in C */
    printf("%-16s -> %s\n", "'a'",        type_name('a'));
    printf("%-16s -> %s\n", "c (a char)", type_name(c));
    printf("%-16s -> %s\n", "c + 1",      type_name(c + 1));   /* promoted */
    printf("%-16s -> %s\n", "byte",       type_name(byte));    /* uint8_t is a typedef */
    printf("%-16s -> %s\n", "ci (const)", type_name(ci));      /* qualifier dropped */
    printf("%-16s -> %s\n", "arr",        type_name(arr));     /* the array decays */
    /* char *, not const char * */
    printf("%-16s -> %s\n", "\"text\"",   type_name("text"));
    printf("%-16s -> %s\n", "1.0f * 2",   type_name(1.0f * 2));

    printf("\nprint: ");
    print(42); printf(", "); print(3u); printf(", ");
    print(2.5); printf(", "); print("hi");
    printf("\n");
    /* print(c) would not compile: there is no char branch. A missing branch is an
       error, which is the good news; the bad news is that `c + 1` silently goes
       to the int one. */
    return 0;
}
