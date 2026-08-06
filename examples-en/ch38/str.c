#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Korean text is kept here on purpose: the point is that a character
       and a byte are not the same thing. */
    char greet[] = "안녕";      /* two characters — but how many bytes? */

    printf("strlen(\"안녕\") = %zu\n", strlen(greet));

    for (size_t i = 0; i < strlen(greet); i += 1) {
        printf("%02X ", (unsigned char)greet[i]);
    }
    printf("\n");

    printf("sizeof greet = %zu (the NUL terminator included)\n", sizeof greet);
    return 0;
}
