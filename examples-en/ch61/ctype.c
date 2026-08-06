#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

/* The ctype functions take an int — but that int is not a char */
int main(void)
{
    printf("CHAR_MIN = %d (char on this machine is %s)\n",
           CHAR_MIN, CHAR_MIN < 0 ? "signed" : "unsigned");

    /* the ASCII range gives no trouble */
    printf("isalpha('A') = %d, isdigit('7') = %d, isspace(' ') = %d\n",
           isalpha('A') != 0, isdigit('7') != 0, isspace(' ') != 0);

    /* bytes of 128 and above are the trouble: passed as a char they go negative */
    char bytes[] = { (char)0xC7, (char)0x41, 'A', '\0' };   /* the first byte of the Korean syllable 가 in CP949 */
    printf("byte 0xC7 held in a char: %d\n", bytes[0]);

    /* the right idiom: convert to unsigned char before passing */
    printf("the right call: isalpha((unsigned char)b) = %d\n",
           isalpha((unsigned char)bytes[0]) != 0);

    /* toupper and tolower follow the same rule */
    const char *s = "Hello, World!";
    char up[32];
    size_t i = 0;
    for (; s[i] && i + 1 < sizeof up; i++)
        up[i] = (char)toupper((unsigned char)s[i]);
    up[i] = '\0';
    printf("toupper applied: [%s]\n", up);

    /* EOF is a valid argument too — which is why the parameter type is int */
    printf("isalpha(EOF) = %d (EOF is an allowed argument)\n", isalpha(EOF) != 0);
    return 0;
}
