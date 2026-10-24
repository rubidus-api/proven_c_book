/* Reading UTF-8 by hand — the rule, and what must be rejected (RFC 3629). */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* The whole rule of UTF-8 fits in one table.
     0xxxxxxx                            -> 1 byte,  U+0000..U+007F
     110xxxxx 10xxxxxx                   -> 2 bytes, U+0080..U+07FF
     1110xxxx 10xxxxxx 10xxxxxx          -> 3 bytes, U+0800..U+FFFF
     11110xxx 10xxxxxx 10xxxxxx 10xxxxxx -> 4 bytes, U+10000..U+10FFFF
   A trail byte is always 10xxxxxx. So from any byte you can tell whether you
   are at the start of a character or inside one — that is self-synchronisation. */

typedef enum { OK, BAD_LEAD, BAD_TRAIL, TRUNCATED, OVERLONG, SURROGATE, TOO_BIG } verdict;

static const char *why(verdict v)
{
    switch (v) {
    case OK:        return "fine";
    case BAD_LEAD:  return "not a value that can lead a character";
    case BAD_TRAIL: return "a trail byte is not 10xxxxxx";
    case TRUNCATED: return "not enough bytes";
    case OVERLONG:  return "overlong — this could be written shorter";
    case SURROGATE: return "surrogates (U+D800..DFFF) cannot be encoded";
    case TOO_BIG:   return "beyond U+10FFFF";
    }
    return "?";
}

/* Read one character. On success the byte count goes into *len. */
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
    else return BAD_LEAD;          /* starts 10xxxxxx, or 11111xxx */

    if (n < need) return TRUNCATED;
    for (size_t i = 1; i < need; i++) {
        if ((s[i] & 0xC0) != 0x80) return BAD_TRAIL;
        v = (v << 6) | (unsigned long)(s[i] & 0x3Fu);
    }

    /* From here on: shapes that parse but must not be accepted */
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
            printf("  %zu byte%s -> U+%04lX\n", len, len == 1 ? "" : "s", cp);
            pos += len;
        } else {
            printf("  rejected: %s\n", why(v));
            pos += 1;                /* drop one byte and resynchronise */
            break;
        }
    }
}

int main(void)
{
    /* the good ones */
    scan("ASCII", (const unsigned char *)"Hi", 2);
    scan("Hangul U+D55C", (const unsigned char *)"\xED\x95\x9C", 3);
    scan("emoji", (const unsigned char *)"\xF0\x9F\x98\x80", 4);

    /* the ones that must be rejected */
    scan("starts on a trail byte", (const unsigned char *)"\x9C", 1);
    scan("a truncated three-byte", (const unsigned char *)"\xED\x95", 2);
    scan("a bad trail byte", (const unsigned char *)"\xED\x41\x9C", 3);
    scan("overlong C0 80", (const unsigned char *)"\xC0\x80", 2);
    scan("surrogate ED A0 80", (const unsigned char *)"\xED\xA0\x80", 3);
    scan("out of range F5 80 80 80", (const unsigned char *)"\xF5\x80\x80\x80", 4);

    puts("\nSelf-synchronisation — point at any byte and you know where you are:");
    const unsigned char *s = (const unsigned char *)"a한글b";
    size_t n = strlen((const char *)s);
    for (size_t i = 0; i < n; i++)
        printf("  byte %zu (%02X): %s\n", i, s[i],
               (s[i] & 0xC0) == 0x80 ? "inside a character" : "start of a character");
    return 0;
}
