/* C23 의 constexpr — 「진짜 상수」가 어디까지 되는가. */
#include <stdio.h>

constexpr int table_size = 4;          /* 파일 스코프: 정적 저장 기간 + 내부 연결 */
constexpr double half = 0.5;
constexpr long long big = 1LL << 40;

int table[table_size];                 /* 배열 크기 — 가변 길이 배열이 아니다 */
static_assert(sizeof table / sizeof table[0] == 4, "table_size is a constant");

constexpr int doubled = table_size * 2;  /* 상수로 상수를 만든다 */

struct limits { int low, high; };
constexpr struct limits range = { 1, 9 };
static_assert(range.high == 9, "a member of a constexpr struct is a constant");

/* 전처리기는 constexpr 을 모른다 — 아래에서 그 사실을 인쇄한다 */
#if table_size == 4
static const char *preproc = "the preprocessor saw table_size == 4";
#else
static const char *preproc = "the preprocessor never saw it: the name became 0";
#endif

static const char *classify(int x)
{
    switch (x) {
    case table_size:  return "exactly the table size";   /* case 라벨 */
    case doubled:     return "twice the table size";
    default:          return "something else";
    }
}

struct packed {
    unsigned flags : table_size;       /* 비트 필드의 폭 */
};

int main(void)
{
    static int copy = table_size + 1;  /* 정적 초기화 */
    enum { same_again = table_size };  /* 열거 상수의 값 */

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

    /* 아래는 전부 컴파일 오류다 — 값이 정확히 표현되어야 하기 때문이다.
           constexpr unsigned int m = -1;      값이 표현 불가
           constexpr float f = 0.1;            double 0.1 은 float 로 정확하지 않다
           constexpr int *q = &copy;           포인터 초기값은 널만 가능
           constexpr const char *s = "abc";    같은 이유로 불가
           constexpr volatile int v = 1;       volatile·restrict·atomic 금지
           constexpr int no_init;              정의이자 초기화여야 한다 */
    return 0;
}
