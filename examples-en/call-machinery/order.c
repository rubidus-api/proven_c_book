// The order in which arguments are evaluated is not fixed by the standard.
// The same source may run in a different order under a different compiler.
#include <stdio.h>

static int step = 0;

// Prints its own name and reports which step it was.
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

    // The order *inside* one argument is another story again.
    step = 0;
    puts("one argument built from two calls:");
    take(mark("left of +") + mark("right of +"), 0, 0);
    return 0;
}
