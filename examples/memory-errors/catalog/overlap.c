/* 겹치는 두 자리 사이의 memcpy --- 겹치면 memmove 를 써야 한다. */
#include <string.h>

[[gnu::noinline]] static void shift(char *s, size_t n) { memcpy(s + 1, s, n); }

int main(void)
{
    char s[16] = "abcdefgh";
    shift(s, 8);            /* s[0..7] 을 s[1..8] 로 --- 두 구역이 겹친다 */
    return s[0];
}
