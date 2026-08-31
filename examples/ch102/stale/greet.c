#include "greet.h"

void greet(char *out)
{
    /* 헤더가 약속한 그릇을 가득 채운다 --- GREET_MAX 를 믿는다 */
    for (int i = 0; i < GREET_MAX - 1; i++)
        out[i] = 'a' + (i % 26);
    out[GREET_MAX - 1] = '\0';
}
