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
        default:            "그 밖")

enum color { RED, GREEN, BLUE };
struct point { int x, y; };
union  bits  { int i; float f; };

int main(void)
{
    puts("[같은 철자라도 컴파일러가 보는 타입은 이것이다]");
    bool          b = true;
    char          c = 'a';
    signed char   sc = 1;
    unsigned char uc = 1;
    uint8_t       u8 = 1;
    enum color    e = RED;

    printf("  bool          → %s\n", TYPE_NAME(b));
    printf("  char          → %s\n", TYPE_NAME(c));
    printf("  signed char   → %s   ← char 와 다른 타입이다\n", TYPE_NAME(sc));
    printf("  unsigned char → %s\n", TYPE_NAME(uc));
    printf("  uint8_t       → %s   ← 새 타입이 아니라 별명이다\n", TYPE_NAME(u8));
    printf("  size_t        → %s\n", TYPE_NAME((size_t)1));
    printf("  열거 변수     → %s\n", TYPE_NAME(e));
    printf("  열거 상수 RED → %s   ← 상수는 int 다\n", TYPE_NAME(RED));

    puts("\n[승격되면 무엇이 되는가 — 산술을 한 번 거치면 드러난다]");
    printf("  char + 0        → %s\n", TYPE_NAME(c + 0));
    printf("  uint8_t + 0     → %s   ← 8비트 산술이 아니다\n", TYPE_NAME(u8 + 0));
    printf("  unsigned char*2 → %s\n", TYPE_NAME(uc * 2));
    printf("  bool + 0        → %s\n", TYPE_NAME(b + 0));

    puts("\n[분류를 크기로 확인한다]");
    printf("  기본 타입은 서로 표현이 같아도 별개 타입이다:\n");
    printf("    sizeof(char)=%zu sizeof(signed char)=%zu sizeof(unsigned char)=%zu\n",
           sizeof(char), sizeof(signed char), sizeof(unsigned char));
    printf("  집합체(배열·구조체)와 공용체:\n");
    printf("    sizeof(struct point)=%zu  sizeof(union bits)=%zu  sizeof(int[3])=%zu\n",
           sizeof(struct point), sizeof(union bits), sizeof(int[3]));
    printf("  스칼라: 산술 타입 + 포인터 + nullptr_t\n");
    printf("    sizeof(void *)=%zu  sizeof(nullptr_t)=%zu\n",
           sizeof(void *), sizeof(nullptr_t));

    puts("\n[void 는 완성될 수 없는 불완전 객체 타입이다]");
    puts("    sizeof(void) 는 표준 C 에서 쓸 수 없다(GCC 는 확장으로 1을 준다).");
    return 0;
}
