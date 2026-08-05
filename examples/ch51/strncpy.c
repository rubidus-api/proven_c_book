#include <stdio.h>
#include <string.h>

static void dump(const char *label, const char *buf, size_t n)
{
    printf("%-14s", label);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 0)       printf(" \\0");
        else if (c >= 32) printf("  %c", c);
        else              printf(" %02x", c);
    }
    printf("\n");
}

/* 컴파일러가 원본 길이를 보지 못하도록 함수 경계 뒤에 둔다.
   (그러지 않으면 gcc 가 -Wstringop-truncation 으로 잡아 준다) */
static void copy_into(char *dst, size_t cap, const char *src)
{
    strncpy(dst, src, cap);
}

int main(void)
{
    /* ① 남는 자리를 전부 0 으로 채운다 — 큰 버퍼에서는 비싸다 */
    char pad[10];
    memset(pad, 'X', sizeof pad);
    copy_into(pad, sizeof pad, "abc");
    dump("짧은 원본:", pad, sizeof pad);

    /* ② 딱 맞거나 넘치면 NUL 을 붙이지 않는다 — 문자열이 아니게 된다 */
    char tight[4];
    memset(tight, 'X', sizeof tight);
    copy_into(tight, sizeof tight, "abcd");
    dump("딱 맞을 때:", tight, sizeof tight);
    printf("               NUL 이 없다 — 이대로 %%s 로 찍으면 계약 밖이다\n");

    /* ③ 안전하게 쓰려면 마지막 칸을 손으로 닫는다 */
    char safe[4];
    copy_into(safe, sizeof safe - 1, "abcd");
    safe[sizeof safe - 1] = '\0';
    printf("직접 닫기    : [%s]\n", safe);

    /* ④ 잘렸는지 알려면 결국 길이를 따로 재야 한다 */
    const char *src = "abcd";
    printf("잘림 여부    : %s\n", strlen(src) >= sizeof safe ? "잘렸다" : "온전하다");
    return 0;
}
