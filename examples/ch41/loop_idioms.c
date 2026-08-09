/* 루프의 관용구 몇 가지 — 그리고 조용히 무는 자리들. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    int a[] = { 10, 20, 30, 40, 50 };
    size_t n = sizeof a / sizeof a[0];

    puts("[역순 순회 — size_t 로는 i >= 0 을 쓸 수 없다]");
    printf("  바른 꼴 (i-- > 0): ");
    for (size_t i = n; i-- > 0; ) printf("%d ", a[i]);
    puts("");
    puts("  잘못된 꼴 (size_t i = n-1; i >= 0; i--) 은 무한 루프다 —");
    puts("  부호 없는 값은 0 다음이 SIZE_MAX 라 조건이 영원히 참이다.");
    printf("  부호 있는 첨자로 쓰면 그 꼴도 된다: ");
    for (int i = (int)n - 1; i >= 0; i--) printf("%d ", a[i]);
    puts("");

    puts("\n[쉼표 연산자 — 두 끝에서 함께 좁혀 온다]");
    printf("  뒤집기: ");
    int b[] = { 1, 2, 3, 4, 5, 6 };
    size_t m = sizeof b / sizeof b[0];
    for (size_t i = 0, j = m - 1; i < j; i++, j--) {
        int t = b[i]; b[i] = b[j]; b[j] = t;
    }
    for (size_t i = 0; i < m; i++) printf("%d ", b[i]);
    puts("");

    puts("\n[센티널 루프 — 읽으면서 동시에 검사한다]");
    const char *text = "hi!\n";
    const char *p = text;
    int c, count = 0;
    while ((c = *p++) != '\0') {          /* 괄호가 필수다 */
        if (c == '\n') printf("\\n ");
        else printf("%c ", c);
        count++;
    }
    printf("\n  %d 글자를 읽었다\n", count);

    puts("\n[루프 조건에서 strlen 을 부르지 않는다]");
    const char *s = "measure once";
    size_t len = strlen(s);               /* 한 번만 잰다 */
    size_t vowels = 0;
    for (size_t i = 0; i < len; i++)
        if (strchr("aeiou", s[i]) && s[i]) vowels++;
    printf("  \"%s\" 의 모음 %zu 개 (길이는 %zu, 한 번만 쟀다)\n", s, vowels, len);

    puts("\n[빈 몸통은 의도를 적어 둔다]");
    size_t skip = 0;
    const char *q = "   값";
    while (q[skip] == ' ')
        skip++;                            /* 몸통이 있는 형태로 적는다 */
    printf("  앞 공백 %zu 칸을 건너뛰었다\n", skip);
    puts("  while (q[skip++] == ' ') ;  처럼 빈 몸통으로 적으면 짧지만,");
    puts("  세미콜론 하나가 눈에 안 띄어 사고가 난다.");
    return 0;
}
