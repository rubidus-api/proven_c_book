/* mbrtowc — 바이트열이 한 글자씩 wchar_t 로 풀리는 과정을 한 단계씩. */
#include <errno.h>
#include <locale.h>
#include <stdlib.h>          /* MB_CUR_MAX */
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* 반환값의 네 갈래를 사람 말로 옮긴다 */
static const char *explain(size_t r)
{
    if (r == 0)           return "hit the null character (returns 0)";
    if (r == (size_t)-1)  return "invalid sequence (errno=EILSEQ)";
    if (r == (size_t)-2)  return "not a whole character yet - more bytes needed";
    return "consumed this many bytes and produced one character";
}

static void walk(const char *label, const char *s, size_t len)
{
    mbstate_t st;
    memset(&st, 0, sizeof st);      /* 초기 변환 상태 */

    printf("\n[%s] %zu bytes:", label, len);
    for (size_t i = 0; i < len; i++) printf(" %02X", (unsigned char)s[i]);
    puts("");

    size_t pos = 0;
    while (pos < len) {
        wchar_t wc = 0;
        errno = 0;
        size_t r = mbrtowc(&wc, s + pos, len - pos, &st);

        printf("  at %zu: mbrtowc -> ", pos);
        if (r == (size_t)-1)      printf("(size_t)-1   ");
        else if (r == (size_t)-2) printf("(size_t)-2   ");
        else                      printf("%-12zu", r);
        printf("%s", explain(r));

        if (r != (size_t)-1 && r != (size_t)-2)
            printf("  → U+%04lX", (unsigned long)wc);
        puts("");

        if (r == (size_t)-1 || r == (size_t)-2) break;
        pos += (r == 0) ? 1 : r;
    }
}

int main(void)
{
    /* 이 예제는 UTF-8 로케일이 필요하다 — 없으면 그 사실을 밝히고 끝낸다 */
    const char *loc = setlocale(LC_CTYPE, "C.UTF-8");
    if (!loc) loc = setlocale(LC_CTYPE, "en_US.UTF-8");
    if (!loc) { puts("no UTF-8 locale - skipping this demonstration"); return 0; }
    printf("LC_CTYPE=%s, MB_CUR_MAX=%zu\n", loc, (size_t)MB_CUR_MAX);

    walk("two ASCII characters", "Hi", 2);
    walk("one Hangul syllable", "한", 3);
    walk("an emoji outside the BMP", "\xF0\x9F\x98\x80", 4);

    /* 잘린 문자: 세 바이트 중 둘만 준다 → (size_t)-2 */
    walk("a character cut in half", "\xED\x95", 2);

    /* 부정한 시퀀스: 후행 바이트로 시작 → (size_t)-1 */
    walk("an invalid sequence", "\x9C\x41", 2);

    /* 상태는 이어진다: 잘린 조각을 두 번에 나눠 넣어도 이어 붙는다 */
    puts("\n[when it arrives in pieces, as from a stream] feeding \"한\" as 2+1 bytes");
    mbstate_t st;
    memset(&st, 0, sizeof st);
    wchar_t wc = 0;
    size_t r1 = mbrtowc(&wc, "\xED\x95", 2, &st);
    printf("  first pass (2 bytes): %s\n", explain(r1));
    size_t r2 = mbrtowc(&wc, "\x9C", 1, &st);
    printf("  second pass (1 byte): %s -> U+%04lX\n", explain(r2), (unsigned long)wc);
    puts("  mbstate_t remembers how far it has seen, so the pieces join up.");
    return 0;
}
