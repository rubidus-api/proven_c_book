#include "greet.h"

void greet(char *out)
{
    /* Fill the container the header promised - trusting GREET_MAX. */
    for (int i = 0; i < GREET_MAX - 1; i++)
        out[i] = 'a' + (i % 26);
    out[GREET_MAX - 1] = '\0';
}
