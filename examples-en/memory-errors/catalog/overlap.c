/* memcpy between two overlapping places --- when they overlap, memmove is required. */
#include <string.h>

[[gnu::noinline]] static void shift(char *s, size_t n) { memcpy(s + 1, s, n); }

int main(void)
{
    char s[16] = "abcdefgh";
    shift(s, 8);            /* s[0..7] onto s[1..8] --- the two regions overlap */
    return s[0];
}
