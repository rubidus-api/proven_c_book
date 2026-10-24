// 원형이 있으면 인자는 "대입하듯" 변환되어 들어간다.
#include <stdio.h>

static void takes_int(int n)
{
    printf("  the parameter holds %d\n", n);
}

static void takes_unsigned(unsigned n)
{
    printf("  the parameter holds %u\n", n);
}

// 반환값도 마찬가지다 --- 돌려주는 식은 반환 타입으로 변환된다.
static int truncating_return(void)
{
    return (int)3.99;              // 명시적으로 적어 두면 읽는 사람이 안다
}

static char narrowing_return(void)
{
    int wide = 321;
    return (char)wide;             // 321 은 char 에 들어가지 않는다
}

int main(void)
{
    puts("a double handed to an int parameter:");
    takes_int(3.7);                // 3 으로 잘려서 들어간다

    puts("a negative int handed to an unsigned parameter:");
    takes_unsigned(-1);            // 감싸 올라간다

    printf("returning 3.99 as int: %d\n", truncating_return());
    printf("returning 321 as char: %d\n", narrowing_return());

    // 원형이 없으면 이 조율이 일어나지 않는다. C23 부터는 원형 없는 호출
    // 자체가 오류이므로, 남은 "조율 없는 자리"는 ... 뿐이다.
    return 0;
}
