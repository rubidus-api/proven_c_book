// Two files know the same function differently.
//
// Warning: this program is undefined behaviour. Neither the compiler nor the
// linker says a word --- each of them sees only its own file. The output
// below is not "the right answer": it is what this machine's calling
// convention happened to make of it.
#include <stdio.h>

// The definition takes two ints; this file declares two longs.
void report(long id, long value);

int main(void)
{
    puts("calling a function this file has mis-declared:");
    report(1, 2);
    puts("it linked, it ran, and it even looks right --- that is the danger.");
    return 0;
}
