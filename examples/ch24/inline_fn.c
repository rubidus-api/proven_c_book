/* 함수 지정자 inline — 무엇을 약속하고 무엇을 약속하지 않는가.
   실무에서 통하는 두 형태만 쓰고, 나머지는 주석으로 이유를 남긴다. */
#include <stdio.h>

/* ① 한 파일 안에서 쓰는 도우미 — 이 형태가 가장 안전하다.
      static 이므로 이 번역 단위의 정의이고, 외부 정의를 따로 둘 필요가 없다. */
static inline int square(int x) { return x * x; }

/* ② 헤더에 두고 여러 파일에서 쓰는 형태.
      아래 두 줄이 짝이다 — inline 정의 하나 + extern 선언 하나.
      extern 선언이 "이 번역 단위가 외부 정의를 낸다"고 말해 준다. */
inline int cube(int x) { return x * x * x; }
extern int cube(int x);

int main(void)
{
    printf("① static inline square(5) = %d\n", square(5));
    printf("② inline + extern  cube(3) = %d\n", cube(3));

    /* ③ 주소를 얻을 수 있다 — 함수는 함수다.
          다만 주소를 얻는 순간 「어딘가에 실물이 하나 있어야」 한다. */
    int (*f)(int) = square;
    int (*g)(int) = cube;
    printf("③ calling through a pointer: f(6)=%d  g(2)=%d\n", f(6), g(2));

    /* ④ inline 은 「이렇게 해 달라」는 부탁일 뿐, 약속이 아니다.
          컴파일러는 펼칠 수도 있고 안 펼칠 수도 있다. */
    printf("④ same answer, inlined or not: %d %d\n", square(4), f(4));

    return 0;
}
