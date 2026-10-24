// With a prototype in scope, arguments are converted "as if by assignment".
#include <stdio.h>

static void takes_int(int n)
{
    printf("  the parameter holds %d\n", n);
}

static void takes_unsigned(unsigned n)
{
    printf("  the parameter holds %u\n", n);
}

// The same holds for the result --- the returned expression is converted
// to the return type.
static int truncating_return(void)
{
    return (int)3.99;              // spelling it out tells the reader
}

static char narrowing_return(void)
{
    int wide = 321;
    return (char)wide;             // 321 does not fit in a char
}

int main(void)
{
    puts("a double handed to an int parameter:");
    takes_int(3.7);                // arrives cut down to 3

    puts("a negative int handed to an unsigned parameter:");
    takes_unsigned(-1);            // wraps around

    printf("returning 3.99 as int: %d\n", truncating_return());
    printf("returning 321 as char: %d\n", narrowing_return());

    // Without a prototype this adjustment does not happen. Since C23 a call
    // with no prototype is an error, so the only unadjusted place left is ... .
    return 0;
}
