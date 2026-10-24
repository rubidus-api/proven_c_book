/* the thing under test - count lines, words and characters */
#include <stdio.h>

int main(void)
{
    long lines = 0, words = 0, chars = 0;
    int c, in_word = 0;

    while ((c = getchar()) != EOF) {
        chars++;
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    printf("%ld %ld %ld\n", lines, words, chars);
    return 0;
}
