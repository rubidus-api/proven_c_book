/* 타입 한정자 넷을 한자리에서 확인한다.
   요점은 「한정판은 같은 타입의 다른 판일 뿐, 크기도 정렬도 바뀌지 않는다」이다. */
#include <stdio.h>
#include <stdalign.h>

struct big { char a[9]; };

int main(void)
{
    puts("① 한정자는 크기도 정렬도 바꾸지 않는다");
    printf("  int            : %zu / %zu\n", sizeof(int), alignof(int));
    printf("  const int      : %zu / %zu\n", sizeof(const int), alignof(const int));
    printf("  volatile int   : %zu / %zu\n", sizeof(volatile int), alignof(volatile int));
    printf("  struct big     : %zu / %zu\n", sizeof(struct big), alignof(struct big));
    printf("  _Atomic big    : %zu / %zu   <- 이 구현에서는 그대로다\n",
           sizeof(_Atomic struct big), alignof(_Atomic struct big));

    puts("\n② 순서는 자유롭고, 겹쳐 쓸 수 있다");
    const volatile int a = 1;
    volatile const int b = 2;      /* 위와 완전히 같은 타입 */
    printf("  const volatile int a = %d,  volatile const int b = %d\n", a, b);

    puts("\n③ _Atomic 은 두 얼굴을 갖는다");
    _Atomic int   c = 3;           /* 한정자로 쓴 꼴 */
    _Atomic(int)  d = 4;           /* 타입 지정자로 쓴 꼴 — 같은 타입이다 */
    printf("  _Atomic int c = %d,  _Atomic(int) d = %d\n", c, d);
    /* typedef int A[3];  _Atomic A x;   <- 배열에는 붙일 수 없다: 컴파일 오류 */

    puts("\n④ const 는 「이 이름으로는 못 바꾼다」일 뿐이다");
    int  x  = 10;
    const int *p = &x;             /* p 로는 못 바꾼다 */
    x = 20;                        /* 그러나 x 로는 바꿀 수 있다 */
    printf("  x 를 직접 고치니 *p = %d  <- const 는 값의 성질이 아니라 통로의 약속\n", *p);
    /* *p = 30;  <- 오류: assignment of read-only location */

    puts("\n⑤ 한정자는 붙일 수는 있어도 몰래 뗄 수는 없다");
    const int ci = 7;
    const int *q = &ci;            /* 붙이기: 된다 */
    printf("  int * -> const int * 는 통과, 반대는 경고(-Wdiscarded-qualifiers), *q = %d\n", *q);
    /* int *bad = &ci;  <- 경고: initialization discards 'const' qualifier */

    return 0;
}
