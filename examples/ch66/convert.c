#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

/* 문자열을 수로 바꾸는 세 가지 방법과 그 차이 */
static void try_atoi(const char *s)
{
    printf("  atoi(\"%s\") = %d\n", s, atoi(s));   /* 실패를 알릴 방법이 없다 */
}

static void try_strtol(const char *s)
{
    errno = 0;
    char *end;
    long v = strtol(s, &end, 10);

    if (end == s)                 printf("  strtol(\"%s\"): not a number\n", s);
    else if (errno == ERANGE)     printf("  strtol(\"%s\"): out of range (ERANGE)\n", s);
    else if (*end != '\0')        printf("  strtol(\"%s\"): %ld, characters left [%s]\n", s, v, end);
    else                          printf("  strtol(\"%s\"): %ld (clean)\n", s, v);
}

int main(void)
{
    const char *cases[] = { "42", "abc", "42abc", "99999999999999999999", " 7", "" };

    printf("atoi - it cannot tell failure from 0:\n");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) try_atoi(cases[i]);

    printf("strtol - it tells you what went wrong:\n");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) try_strtol(cases[i]);

    printf("LONG_MAX = %ld\n", LONG_MAX);
    return 0;
}
