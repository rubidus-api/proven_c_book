// 인자가 평가되는 순서는 표준이 정해 두지 않았다.
// 같은 소스가 컴파일러마다 다른 순서로 돌 수 있다.
#include <stdio.h>

static int step = 0;

// 부르면 자기 이름을 찍고 몇 번째로 불렸는지를 돌려준다.
static int mark(const char *who)
{
    step += 1;
    printf("  evaluated %s (step %d)\n", who, step);
    return step;
}

static void take(int a, int b, int c)
{
    printf("  the callee got a=%d b=%d c=%d\n", a, b, c);
}

int main(void)
{
    puts("call with three arguments:");
    take(mark("the first argument"),
         mark("the second argument"),
         mark("the third argument"));

    // 한 인자 *안에서*의 순서는 또 다른 이야기다.
    step = 0;
    puts("one argument built from two calls:");
    take(mark("left of +") + mark("right of +"), 0, 0);
    return 0;
}
