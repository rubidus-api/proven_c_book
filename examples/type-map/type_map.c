/* 표준의 타입 분류를 컴파일러에게 직접 물어본다.
   _Generic 은 '이 수식의 타입이 무엇이냐'를 컴파일 시간에 고르는 장치다. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 어떤 타입인지 이름을 돌려준다 — 목록에 없으면 "그 밖" */
#define TYPE_NAME(x) _Generic((x),                    \
        _Bool:              "bool",                   \
        char:               "char",                   \
        signed char:        "signed char",            \
        unsigned char:      "unsigned char",          \
        short:              "short",                  \
        unsigned short:     "unsigned short",         \
        int:                "int",                    \
        unsigned int:       "unsigned int",           \
        long:               "long",                   \
        unsigned long:      "unsigned long",          \
        long long:          "long long",              \
        unsigned long long: "unsigned long long",     \
        float:              "float",                  \
        double:             "double",                 \
        long double:        "long double",            \
        void *:             "void *",                 \
        default:            "other")

enum color { RED, GREEN, BLUE };
struct point { int x, y; };
union  bits  { int i; float f; };

int main(void)
{
    puts("[same spelling, but this is the type the compiler sees]");
    bool          b = true;
    char          c = 'a';
    signed char   sc = 1;
    unsigned char uc = 1;
    uint8_t       u8 = 1;
    enum color    e = RED;

    printf("  bool          → %s\n", TYPE_NAME(b));
    printf("  char          → %s\n", TYPE_NAME(c));
    printf("  signed char   -> %s   <- a different type from char\n", TYPE_NAME(sc));
    printf("  unsigned char → %s\n", TYPE_NAME(uc));
    printf("  uint8_t       -> %s   <- an alias, not a new type\n", TYPE_NAME(u8));
    printf("  size_t        → %s\n", TYPE_NAME((size_t)1));
    printf("  enum variable -> %s\n", TYPE_NAME(e));
    printf("  enum constant -> %s   <- RED itself is int\n", TYPE_NAME(RED));

    puts("\n[what promotion turns them into - one arithmetic step reveals it]");
    printf("  char + 0        → %s\n", TYPE_NAME(c + 0));
    printf("  uint8_t + 0     -> %s   <- not 8-bit arithmetic\n", TYPE_NAME(u8 + 0));
    printf("  unsigned char*2 → %s\n", TYPE_NAME(uc * 2));
    printf("  bool + 0        → %s\n", TYPE_NAME(b + 0));

    puts("\n[checking the families by size]");
    printf("  basic types are distinct even when their representations match:\n");
    printf("    sizeof(char)=%zu sizeof(signed char)=%zu sizeof(unsigned char)=%zu\n",
           sizeof(char), sizeof(signed char), sizeof(unsigned char));
    printf("  aggregates (arrays, structs) and unions:\n");
    printf("    sizeof(struct point)=%zu  sizeof(union bits)=%zu  sizeof(int[3])=%zu\n",
           sizeof(struct point), sizeof(union bits), sizeof(int[3]));
    printf("  scalars: arithmetic types + pointers + nullptr_t\n");
    printf("    sizeof(void *)=%zu  sizeof(nullptr_t)=%zu\n",
           sizeof(void *), sizeof(nullptr_t));

    puts("\n[void is an incomplete object type that can never be completed]");
    puts("    sizeof(void) is not allowed in standard C (GCC gives 1 as an extension).");
    return 0;
}
