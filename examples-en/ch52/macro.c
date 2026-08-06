#include <stdio.h>

/* # : makes the argument into a string "letter for letter" (stringize) */
#define SHOW(expr)  printf(#expr " = %d\n", (expr))

/* ## : pastes two tokens into one (token paste) */
#define MAKE_NAME(prefix, n)  prefix##n

/* one layer does not expand the macro — the double indirection is the canonical way */
#define STR_RAW(x)  #x
#define STR(x)      STR_RAW(x)
#define WIDTH       80

int MAKE_NAME(value_, 1) = 11;   /* -> int value_1 = 11; */
int MAKE_NAME(value_, 2) = 22;

int main(void)
{
    SHOW(2 + 3 * 4);
    SHOW(value_1 + value_2);

    printf("STR_RAW(WIDTH) = %s\n", STR_RAW(WIDTH));  /* not expanded */
    printf("STR(WIDTH)     = %s\n", STR(WIDTH));      /* expanded first, then made a string */
    return 0;
}
