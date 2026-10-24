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
    case OK:        return "ok";
    case BAD_LEAD:  return "a value that cannot be a lead byte";
    case BAD_TRAIL: return "a trail byte that is not 10xxxxxx";
    case TRUNCATED: return "not enough bytes";
    case OVERLONG:  return "overlong encoding - the value could be written shorter";
    case SURROGATE: return "surrogates (U+D800-DFFF) cannot be encoded";
    case TOO_BIG:   return "beyond U+10FFFF";
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
            printf("  %zu bytes -> U+%04lX\n", len, cp);
            pos += len;
        } else {
            printf("  rejected: %s\n", why(v));
            pos += 1;                /* 한 바이트만 버리고 다시 맞춘다 */
            break;
        }
    }
}

int main(void)
{
    /* 정상 */
    scan("ASCII", (const unsigned char *)"Hi", 2);
    scan("the Hangul syllable '한'", (const unsigned char *)"\xED\x95\x9C", 3);
    scan("emoji", (const unsigned char *)"\xF0\x9F\x98\x80", 4);

    /* 거절해야 하는 것들 */
    scan("starts with a trail byte", (const unsigned char *)"\x9C", 1);
    scan("three bytes cut short", (const unsigned char *)"\xED\x95", 2);
    scan("a byte that is not a trail byte", (const unsigned char *)"\xED\x41\x9C", 3);
    scan("overlong encoding C0 80", (const unsigned char *)"\xC0\x80", 2);
    scan("surrogate ED A0 80", (const unsigned char *)"\xED\xA0\x80", 3);
    scan("out of range F5 80 80 80", (const unsigned char *)"\xF5\x80\x80\x80", 4);

    puts("\nself-synchronization - point at any byte and you can find the character boundary:");
    const unsigned char *s = (const unsigned char *)"a한글b";
    size_t n = strlen((const char *)s);
    for (size_t i = 0; i < n; i++)
        printf("  byte %zu (%02X): %s\n", i, s[i],
               (s[i] & 0xC0) == 0x80 ? "inside a character" : "start of a character");
    return 0;
}
