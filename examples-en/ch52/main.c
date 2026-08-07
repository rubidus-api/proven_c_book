/* A multi-file example — this file compiles seeing only greet's declaration. */
#include "greet.h"

int main(void)
{
    greet("world");
    printf_count();
    return 0;
}
