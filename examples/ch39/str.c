#include <stdio.h>
#include <string.h>

int main(void)
{
    char greet[] = "안녕";      /* 글자 2개 — 그러나 바이트는? */

    printf("strlen(\"안녕\") = %zu\n", strlen(greet));

    for (size_t i = 0; i < strlen(greet); i += 1) {
        printf("%02X ", (unsigned char)greet[i]);
    }
    printf("\n");

    printf("sizeof greet = %zu (NUL 종단 포함)\n", sizeof greet);
    return 0;
}
