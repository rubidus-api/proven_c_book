/* 선언을 읽는 두 가지 방법을 눈으로 확인한다.
   같은 뜻을 typedef 로 쌓아 만든 타입과 원래 선언이 정말 같은지도 검사한다. */
#include <stdio.h>

/* ① 별 하나 차이 ─────────────────────────────────────── */
int  *pa[3];      /* pa: 배열[3] of 포인터 to int   */
int (*ap)[3];     /* ap: 포인터 to 배열[3] of int   */

/* ② 함수가 끼면 ──────────────────────────────────────── */
int  *f(void);    /* f: 함수(void) 반환 포인터 to int */
int (*g)(void);   /* g: 포인터 to 함수(void) 반환 int */

/* ③ 악명 높은 형태: 배열[4] of 포인터 to 함수(int) 반환 포인터 to char */
char *(*table[4])(int);

/* ④ 같은 타입을 typedef 로 한 겹씩 쌓아 만든다 */
typedef char           *charptr;        /* 포인터 to char                  */
typedef charptr         handler(int);   /* 함수(int) 반환 charptr          */
typedef handler        *handler_ptr;    /* 포인터 to 그 함수               */
typedef handler_ptr     table4[4];      /* 배열[4] of 그 포인터            */

/* 두 길로 만든 타입이 정말 같은지 컴파일 시간에 검사한다 */
static_assert(sizeof(table4) == sizeof(table), "they must be the same type");

static char *shout(int n)  { (void)n; return "shout"; }
static char *quiet(int n)  { (void)n; return "quiet"; }

int main(void)
{
    printf("int  *pa[3]  : whole %zu, element %zu  -> %zu pointers\n",
           sizeof pa, sizeof pa[0], sizeof pa / sizeof pa[0]);
    printf("int (*ap)[3] : whole %zu, pointee %zu\n",
           sizeof ap, sizeof *ap);

    /* ③ 을 실제로 채워 쓴다 */
    table[0] = shout;
    table[1] = quiet;
    printf("table[0](1) = %s, table[1](2) = %s\n", table[0](1), table[1](2));

    /* ④ 의 typedef 로 만든 변수도 같은 자리에 그대로 들어간다 */
    table4 other = { quiet, shout };
    printf("other[0](3) = %s   (the same type, made with a typedef)\n", other[0](3));

    /* 식별자가 없는 형태(추상 선언자): 캐스트와 sizeof 에서 쓴다 */
    printf("sizeof(char *(*)(int)) = %zu   (an unnamed function pointer type)\n",
           sizeof(char *(*)(int)));
    return 0;
}
