/* 저장 클래스 지정자가 정하는 것을 눈으로 확인한다.
   한 선언에 이 자리는 하나뿐이라는 규칙과, 그 유일한 예외도 함께 본다. */
#include <stdio.h>

static int file_only = 1;          /* 내부 연결 — 이 파일 안에서만 보인다 */
extern int shared;                 /* 예고만 — 실물은 아래에 있다 */
int        shared = 2;             /* 정의 */
thread_local int per_thread = 3;   /* 갈래마다 따로 */

static int bump(void)
{
    static int calls = 0;          /* 정적 저장 기간 — 호출 사이에 살아남는다 */
    return ++calls;
}

int main(void)
{
    auto int local = 4;            /* C17 까지의 뜻: 자동 저장 기간(기본값) */
    register int hot = 5;          /* 옛 부탁. 주소는 못 얻는다 */
    constexpr int fixed = 6;       /* C23 — 컴파일 시간 상수 */
    static_assert(fixed == 6, "constexpr is a constant expression");

    printf("file_only=%d shared=%d per_thread=%d local=%d hot=%d fixed=%d\n",
           file_only, shared, per_thread, local, hot, fixed);
    /* 한 문장에 여러 번 부르면 평가 순서가 미지정이라 순서가 뒤집힌다(34장).
       그래서 한 줄에 하나씩 부른다. */
    int b1 = bump(), b2 = bump(), b3 = bump();
    printf("bump() three times: %d %d %d   <- a static local survives\n", b1, b2, b3);

    /* static extern int bad;   <- 오류: multiple storage classes            */
    /* int *p = &hot;           <- 오류: address of register variable        */
    /* auto constexpr int c=1;  <- 오류: 'auto' used with 'constexpr'        */
    static thread_local int ok = 7;   /* 이 조합만 예외로 허용된다 */
    printf("static thread_local ok=%d\n", ok);
    return 0;
}
