// A function that returns a function pointer --- one thing, three spellings.
#include <stdio.h>

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }

// (1) The raw declarator. Read it from the inside out:
//     pick is a function taking (char), and its result is a pointer to
//     a function taking (int, int) and giving an int.
static int (*pick(char op))(int, int)
{
    return op == '+' ? add : mul;
}

// (2) Name the function type and the same thing reads as one line.
typedef int binop(int, int);      // a function type
static binop *pick_typedef(char op)
{
    return op == '+' ? add : mul;
}

// (3) Name only the pointer type --- the common compromise.
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

    // What comes back is a value: keep it and call it later.
    binop_ptr f = pick('*');
    printf("stored and called later: %d\n", f(6, 7));

    // (*f)(...) and f(...) mean the same --- the name decays to a pointer anyway.
    printf("both spellings agree: %d %d\n", (*f)(2, 3), f(2, 3));
    return 0;
}
