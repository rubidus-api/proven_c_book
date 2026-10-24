// 함수 포인터를 돌려주는 함수 --- 같은 것을 세 가지 표기로 적는다.
#include <stdio.h>

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }

// ① 날것의 선언자. 안쪽부터 읽는다:
//    pick 은 (char) 를 받는 함수이고, 그 결과는
//    (int, int) 를 받아 int 를 주는 함수를 가리키는 포인터다.
static int (*pick(char op))(int, int)
{
    return op == '+' ? add : mul;
}

// ② 이름을 붙여 두면 같은 뜻이 한 줄로 읽힌다.
typedef int binop(int, int);      // 함수 타입
static binop *pick_typedef(char op)
{
    return op == '+' ? add : mul;
}

// ③ 반환 타입만 이름 붙이는 흔한 절충.
typedef int (*binop_ptr)(int, int);
static binop_ptr pick_ptr(char op)
{
    return op == '+' ? add : mul;
}

int main(void)
{
    printf("raw declarator: %d\n", pick('+')(3, 4));
    printf("function typedef: %d\n", pick_typedef('*')(3, 4));
    printf("pointer typedef: %d\n", pick_ptr('+')(10, 32));

    // 돌려받은 포인터는 값이므로 변수에 담아 두었다가 나중에 불러도 된다.
    binop_ptr f = pick('*');
    printf("stored and called later: %d\n", f(6, 7));

    // (*f)(...) 와 f(...) 는 같은 뜻이다 --- 이름은 어차피 포인터로 무너진다.
    printf("both spellings agree: %d %d\n", (*f)(2, 3), f(2, 3));
    return 0;
}
