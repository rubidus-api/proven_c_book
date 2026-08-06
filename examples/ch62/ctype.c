#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

/* ctype 함수는 int 를 받는다 — 그런데 그 int 는 char 가 아니다 */
int main(void)
{
    printf("CHAR_MIN = %d (이 기계의 char 는 %s)\n",
           CHAR_MIN, CHAR_MIN < 0 ? "부호 있음" : "부호 없음");

    /* 아스키 범위는 문제가 없다 */
    printf("isalpha('A') = %d, isdigit('7') = %d, isspace(' ') = %d\n",
           isalpha('A') != 0, isdigit('7') != 0, isspace(' ') != 0);

    /* 128 이상의 바이트가 문제다: char 로 넘기면 음수가 된다 */
    char bytes[] = { (char)0xC7, (char)0x41, 'A', '\0' };   /* CP949 '가' 의 첫 바이트 */
    printf("바이트 0xC7 을 char 로 담으면: %d\n", bytes[0]);

    /* 올바른 관용구: unsigned char 로 바꿔서 넘긴다 */
    printf("올바른 호출: isalpha((unsigned char)b) = %d\n",
           isalpha((unsigned char)bytes[0]) != 0);

    /* toupper/tolower 도 같은 규칙 */
    const char *s = "Hello, World!";
    char up[32];
    size_t i = 0;
    for (; s[i] && i + 1 < sizeof up; i++)
        up[i] = (char)toupper((unsigned char)s[i]);
    up[i] = '\0';
    printf("toupper 적용: [%s]\n", up);

    /* EOF 도 유효한 인자다 — 그래서 인자 타입이 int 다 */
    printf("isalpha(EOF) = %d (EOF 는 허용된 인자)\n", isalpha(EOF) != 0);
    return 0;
}
