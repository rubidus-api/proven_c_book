#include <stdio.h>

/* # : 인자를 "글자 그대로" 문자열로 만든다 (stringize) */
#define SHOW(expr)  printf(#expr " = %d\n", (expr))

/* ## : 두 토큰을 하나로 붙인다 (token paste) */
#define MAKE_NAME(prefix, n)  prefix##n

/* 한 겹으로는 매크로가 안 풀린다 — 두 겹 우회가 정석이다 */
#define STR_RAW(x)  #x
#define STR(x)      STR_RAW(x)
#define WIDTH       80

int MAKE_NAME(value_, 1) = 11;   /* -> int value_1 = 11; */
int MAKE_NAME(value_, 2) = 22;

int main(void)
{
    SHOW(2 + 3 * 4);
    SHOW(value_1 + value_2);

    printf("STR_RAW(WIDTH) = %s\n", STR_RAW(WIDTH));  /* 안 풀린다 */
    printf("STR(WIDTH)     = %s\n", STR(WIDTH));      /* 풀린 뒤 문자열이 된다 */
    return 0;
}
