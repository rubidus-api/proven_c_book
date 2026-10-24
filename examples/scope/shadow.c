/* 가림(shadowing), 그리고 전처리기는 스코프를 모른다는 것. */
#include <stdio.h>

int count = 100;                 /* 파일 스코프 */

static void layers(void)
{
    printf("file scope        : count = %d\n", count);
    int count = 10;              /* 파일 스코프의 count 를 가린다 */
    printf("function body     : count = %d\n", count);
    {
        int count = 1;           /* 다시 가린다 */
        printf("inner block       : count = %d\n", count);
    }
    printf("back in the body  : count = %d\n", count);
    /* 가려진 이름은 사라진 것이 아니다 --- 다른 이름으로는 여전히 닿는다 */
}

static void define_inside(void)
{
    /* 이 지시는 '함수 안' 이라는 자리와 아무 상관이 없다.
       전처리는 컴파일 전에 끝나고, 전처리기는 블록을 모른다. */
#define LIMIT 5
    printf("\ninside the function : LIMIT = %d\n", LIMIT);
}

static void far_away(void)
{
    /* 다른 함수인데도 LIMIT 이 그대로 살아 있다 --- 스코프가 아니라
       '그 지점부터 파일 끝까지'이기 때문이다. */
    printf("another function    : LIMIT = %d\n", LIMIT);
}

int main(void)
{
    layers();
    define_inside();
    far_away();
    printf("\nthe file-scope count is still %d\n", count);
    return 0;
}
