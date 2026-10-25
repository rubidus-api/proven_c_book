/* C 의 오버로딩: _Generic 은 식의 타입으로 갈래를 고른다 --- 컴파일 때.
   그것이 보는 타입이 늘 사람이 생각한 타입은 아니다. */
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

/* 타입으로 오버로딩한 「print」 --- 모든 갈래가 호출마다 컴파일되어야 한다 */
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

    /* C 에서 문자 상수는 int 다 */
    printf("%-16s -> %s\n", "'a'",        type_name('a'));
    printf("%-16s -> %s\n", "c (a char)", type_name(c));
    printf("%-16s -> %s\n", "c + 1",      type_name(c + 1));   /* 승격된다 */
    printf("%-16s -> %s\n", "byte",       type_name(byte));    /* uint8_t 는 typedef 다 */
    printf("%-16s -> %s\n", "ci (const)", type_name(ci));      /* 한정자가 떨어진다 */
    printf("%-16s -> %s\n", "arr",        type_name(arr));     /* 배열이 무너진다 */
    /* const char * 가 아니라 char * */
    printf("%-16s -> %s\n", "\"text\"",   type_name("text"));
    printf("%-16s -> %s\n", "1.0f * 2",   type_name(1.0f * 2));

    printf("\nprint: ");
    print(42); printf(", "); print(3u); printf(", ");
    print(2.5); printf(", "); print("hi");
    printf("\n");
    /* print(c) 는 컴파일되지 않는다: char 갈래가 없다. 빠진 갈래가 오류라는 것은
       좋은 소식이고, `c + 1` 이 조용히 int 갈래로 간다는 것은 나쁜 소식이다. */
    return 0;
}
