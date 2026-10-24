/* Buffer overflow --- strcpy 17 bytes (including the null) into an 8-byte container.
 * The source string is chosen at run time, so the compiler cannot notice in advance. */
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    (void)argv;
    char name[8];
    const char *src = argc > 5 ? "ok" : "0123456789abcdef";
    strcpy(name, src);              /* a function that does not take the container's size */
    printf("copied %zu bytes into an 8-byte buffer\n", strlen(name) + 1);
    return 0;
}
