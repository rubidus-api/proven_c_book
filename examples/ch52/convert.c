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

    if (end == s)                 printf("  strtol(\"%s\"): 숫자가 아님\n", s);
    else if (errno == ERANGE)     printf("  strtol(\"%s\"): 범위 밖 (ERANGE)\n", s);
    else if (*end != '\0')        printf("  strtol(\"%s\"): %ld, 남은 글자 [%s]\n", s, v, end);
    else                          printf("  strtol(\"%s\"): %ld (온전)\n", s, v);
}

int main(void)
{
    const char *cases[] = { "42", "abc", "42abc", "99999999999999999999", " 7", "" };

    printf("atoi — 실패와 0 을 구별할 수 없다:\n");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) try_atoi(cases[i]);

    printf("strtol — 무엇이 잘못됐는지 말해 준다:\n");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) try_strtol(cases[i]);

    printf("LONG_MAX = %ld\n", LONG_MAX);
    return 0;
}
