/* UTF-8 을 직접 읽는다 — 규칙, 그리고 거절해야 하는 것들(RFC 3629). */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* UTF-8 의 규칙은 표 하나로 끝난다.
     0xxxxxxx                            → 1바이트, U+0000..U+007F
     110xxxxx 10xxxxxx                   → 2바이트, U+0080..U+07FF
     1110xxxx 10xxxxxx 10xxxxxx          → 3바이트, U+0800..U+FFFF
     11110xxx 10xxxxxx 10xxxxxx 10xxxxxx → 4바이트, U+10000..U+10FFFF
   후행 바이트는 언제나 10xxxxxx 다. 그래서 어느 바이트를 보든
   '글자의 시작인지 중간인지' 알 수 있다 — 이것이 자기동기화다. */

typedef enum { OK, BAD_LEAD, BAD_TRAIL, TRUNCATED, OVERLONG, SURROGATE, TOO_BIG } verdict;

static const char *why(verdict v)
{
    switch (v) {
    case OK:        return "정상";
    case BAD_LEAD:  return "선행 바이트가 될 수 없는 값";
    case BAD_TRAIL: return "후행 바이트가 10xxxxxx 가 아니다";
    case TRUNCATED: return "필요한 바이트가 모자란다";
    case OVERLONG:  return "과잉 인코딩 — 더 짧게 적을 수 있는 값";
    case SURROGATE: return "서러게이트(U+D800~DFFF)는 인코딩할 수 없다";
    case TOO_BIG:   return "U+10FFFF 를 넘는다";
    }
    return "?";
}

/* 한 글자를 읽는다. 성공하면 소비한 바이트 수를 *len 에 넣는다. */
static verdict decode(const unsigned char *s, size_t n,
                      unsigned long *cp, size_t *len)
{
    if (n == 0) return TRUNCATED;
    unsigned char c = s[0];
    size_t need;
    unsigned long v;

    if (c < 0x80)                  { need = 1; v = c; }
    else if ((c & 0xE0) == 0xC0)   { need = 2; v = c & 0x1Fu; }
    else if ((c & 0xF0) == 0xE0)   { need = 3; v = c & 0x0Fu; }
    else if ((c & 0xF8) == 0xF0)   { need = 4; v = c & 0x07u; }
    else return BAD_LEAD;          /* 10xxxxxx 로 시작하거나 11111xxx */

    if (n < need) return TRUNCATED;
    for (size_t i = 1; i < need; i++) {
        if ((s[i] & 0xC0) != 0x80) return BAD_TRAIL;
        v = (v << 6) | (unsigned long)(s[i] & 0x3Fu);
    }

    /* 여기부터가 '문법은 맞지만 받아들이면 안 되는' 것들이다 */
    static const unsigned long lowest[5] = { 0, 0, 0x80, 0x800, 0x10000 };
    if (v < lowest[need])                return OVERLONG;
    if (v >= 0xD800UL && v <= 0xDFFFUL)  return SURROGATE;
    if (v > 0x10FFFFUL)                  return TOO_BIG;

    *cp = v; *len = need;
    return OK;
}

static void scan(const char *label, const unsigned char *s, size_t n)
{
    printf("\n[%s]", label);
    for (size_t i = 0; i < n; i++) printf(" %02X", s[i]);
    puts("");

    for (size_t pos = 0; pos < n; ) {
        unsigned long cp = 0;
        size_t len = 0;
        verdict v = decode(s + pos, n - pos, &cp, &len);
        if (v == OK) {
            printf("  %zu바이트 → U+%04lX\n", len, cp);
            pos += len;
        } else {
            printf("  거절: %s\n", why(v));
            pos += 1;                /* 한 바이트만 버리고 다시 맞춘다 */
            break;
        }
    }
}

int main(void)
{
    /* 정상 */
    scan("아스키", (const unsigned char *)"Hi", 2);
    scan("한글 '한'", (const unsigned char *)"\xED\x95\x9C", 3);
    scan("이모지", (const unsigned char *)"\xF0\x9F\x98\x80", 4);

    /* 거절해야 하는 것들 */
    scan("후행 바이트로 시작", (const unsigned char *)"\x9C", 1);
    scan("잘린 세 바이트", (const unsigned char *)"\xED\x95", 2);
    scan("후행이 아닌 바이트", (const unsigned char *)"\xED\x41\x9C", 3);
    scan("과잉 인코딩 C0 80", (const unsigned char *)"\xC0\x80", 2);
    scan("서러게이트 ED A0 80", (const unsigned char *)"\xED\xA0\x80", 3);
    scan("범위 초과 F5 80 80 80", (const unsigned char *)"\xF5\x80\x80\x80", 4);

    puts("\n자기동기화 확인 — 아무 바이트나 짚어도 글자 경계를 찾을 수 있다:");
    const unsigned char *s = (const unsigned char *)"a한글b";
    size_t n = strlen((const char *)s);
    for (size_t i = 0; i < n; i++)
        printf("  바이트 %zu (%02X): %s\n", i, s[i],
               (s[i] & 0xC0) == 0x80 ? "글자 중간" : "글자 시작");
    return 0;
}
