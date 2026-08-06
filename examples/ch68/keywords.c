/* C23 에서 키워드로 승격된 것들 — 무엇이 달라졌나 */
#include <stdio.h>

/* 승격 전에는 이 줄들이 <stdbool.h>, <stdalign.h>, <assert.h> 를 요구했다 */
static_assert(sizeof(int) >= 4, "이 코드는 32비트 이상 int 를 가정한다");

/* nullptr 은 자기 타입(nullptr_t)을 가진다 — 0 도 (void *)0 도 아니다 */
static void take_ptr(int *p) { printf("  포인터: %s\n", p ? "값 있음" : "널"); }

/* 가변 인자에서 NULL 과 nullptr 의 차이가 드러나는 자리 */
static void print_all(const char *first, ...)
{
    printf("  %s ...\n", first);
}

int main(void)
{
    bool ready = true;                 /* _Bool 도 매크로도 아닌 진짜 키워드 */
    printf("bool 크기 = %zu, true = %d, false = %d\n", sizeof(bool), true, false);
    printf("ready = %d, !ready = %d\n", ready, !ready);

    /* bool 은 0 아닌 값을 전부 1 로 좁힌다 — int 로 흉내 낼 수 없는 성질 */
    bool  b = 42;
    int   i = 42;
    printf("bool b = 42 -> %d,  int i = 42 -> %d\n", b, i);

    int   x  = 7;
    int  *p  = &x;
    int  *np = nullptr;
    take_ptr(p);
    take_ptr(np);
    printf("nullptr 끼리 비교: %d,  포인터와 비교: %d\n", nullptr == nullptr, p == nullptr);

    /* typeof: 이름 없는 타입을 그대로 받아 적는다 */
    typeof(x) y = x * 2;
    printf("typeof(x) y = %d\n", y);

    /* constexpr: 진짜 상수 — 배열 크기에도, switch 라벨에도 쓸 수 있다 */
    constexpr int LANES = 4;
    int lane[LANES];
    printf("constexpr LANES = %d, 배열 원소 수 = %zu\n", LANES, sizeof lane / sizeof lane[0]);

    print_all("가변 인자에는 nullptr 를 넘긴다", nullptr);
    return 0;
}
