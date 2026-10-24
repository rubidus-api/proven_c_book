/* Breaking initialisation, reading --- put a byte that is neither 0 nor 1 (2) where a bool lives, then read it. */
#include <string.h>

[[gnu::noinline]] static bool get(const bool *p) { return *p; }

int main(void)
{
    bool b;
    unsigned char two = 2;
    memcpy(&b, &two, 1);    /* writing it as a byte is still inside the contract */
    return get(&b) ? 0 : 1; /* reading it as bool reads a non-value representation (C23 6.2.6.1p5) */
}
