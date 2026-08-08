/* fgets 의 계약 — 무엇을 담아 주고, 무엇을 알려 주지 않는가. */
#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[16];          /* 일부러 작게 잡았다 — 잘림을 보이려고 */

    puts("[한 줄씩 읽어 그대로 되돌려 준다]");
    while (fgets(line, sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        int has_nl = (len > 0 && line[len - 1] == '\n');

        /* 담긴 것을 눈에 보이게 — 개행은 기호로 바꿔서 */
        printf("  받은 것: \"");
        for (size_t i = 0; i < len; i++)
            putchar(line[i] == '\n' ? '@' : line[i]);
        printf("\"  (%zu바이트, 끝에 개행 %s)\n",
               len, has_nl ? "있음" : "없음");

        if (!has_nl)
            puts("    → 개행이 없다는 것은 '줄이 그릇보다 길어 잘렸다'는 뜻이다");
    }

    puts("\n[읽기가 끝나는 두 가지 이유]");
    puts("  fgets 가 NULL 을 돌려주면 '더 읽을 것이 없거나(EOF) 오류'다.");
    puts("  둘을 가르는 것은 feof 와 ferror 인데, 정식 취급은 62장에 있다.");
    return 0;
}
