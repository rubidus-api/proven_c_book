/* The contract of fgets — what it fills in, and what it does not tell you. */
#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[16];          /* deliberately small — to show truncation */

    puts("[read one line at a time and echo it back]");
    while (fgets(line, sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        int has_nl = (len > 0 && line[len - 1] == '\n');

        /* make what arrived visible — the newline shown as a mark */
        printf("  got: \"");
        for (size_t i = 0; i < len; i++)
            putchar(line[i] == '\n' ? '@' : line[i]);
        printf("\"  (%zu bytes, newline at the end: %s)\n",
               len, has_nl ? "yes" : "no");

        if (!has_nl)
            puts("    -> no newline means 'the line was longer than the box and was cut'");
    }

    puts("\n[two reasons reading ends]");
    puts("  fgets returning NULL means 'nothing left (EOF), or an error'.");
    puts("  feof and ferror tell them apart; chapter 62 treats them properly.");
    return 0;
}
