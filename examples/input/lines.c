/* fgets 의 계약 — 무엇을 담아 주고, 무엇을 알려 주지 않는가. */
#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[16];          /* 일부러 작게 잡았다 — 잘림을 보이려고 */

    puts("[read one line at a time and echo it back]");
    while (fgets(line, sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        int has_nl = (len > 0 && line[len - 1] == '\n');

        /* 담긴 것을 눈에 보이게 — 개행은 기호로 바꿔서 */
        printf("  got: \"");
        for (size_t i = 0; i < len; i++)
            putchar(line[i] == '\n' ? '@' : line[i]);
        printf("\"  (%zu bytes, newline at end: %s)\n",
               len, has_nl ? "yes" : "no");

        if (!has_nl)
            puts("    -> no newline means the line was longer than the buffer and was cut");
    }

    puts("\n[two reasons reading stops]");
    puts("  when fgets returns NULL it is either end of file or an error.");
    puts("  feof and ferror tell them apart; chapter 63 covers that properly.");
    return 0;
}
