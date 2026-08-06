/* mbrtowc — bytes turning into wchar_t, one step at a time. */
#include <errno.h>
#include <locale.h>
#include <stdlib.h>          /* MB_CUR_MAX */
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* The four kinds of return value, in words */
static const char *explain(size_t r)
{
    if (r == 0)           return "hit the null character (returns 0)";
    if (r == (size_t)-1)  return "invalid sequence (errno=EILSEQ)";
    if (r == (size_t)-2)  return "not a whole character yet — needs more";
    return "consumed this many bytes and produced one character";
}

static void walk(const char *label, const char *s, size_t len)
{
    mbstate_t st;
    memset(&st, 0, sizeof st);      /* the initial conversion state */

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
            printf("  -> U+%04lX", (unsigned long)wc);
        puts("");

        if (r == (size_t)-1 || r == (size_t)-2) break;
        pos += (r == 0) ? 1 : r;
    }
}

int main(void)
{
    /* This one needs a UTF-8 locale — if there is none, say so and stop */
    const char *loc = setlocale(LC_CTYPE, "C.UTF-8");
    if (!loc) loc = setlocale(LC_CTYPE, "en_US.UTF-8");
    if (!loc) { puts("no UTF-8 locale — skipping this demonstration"); return 0; }
    printf("LC_CTYPE=%s, MB_CUR_MAX=%zu\n", loc, (size_t)MB_CUR_MAX);

    walk("two ASCII letters", "Hi", 2);
    walk("one Hangul syllable", "한", 3);
    walk("an emoji beyond the BMP", "\xF0\x9F\x98\x80", 4);

    /* A truncated character: two of three bytes -> (size_t)-2 */
    walk("a truncated character", "\xED\x95", 2);

    /* An invalid sequence: starting on a trail byte -> (size_t)-1 */
    walk("an invalid sequence", "\x9C\x41", 2);

    /* The state carries over: feed the pieces in two calls and they join */
    puts("\n[arriving in pieces, as from a stream] \"한\" fed as 2+1 bytes");
    mbstate_t st;
    memset(&st, 0, sizeof st);
    wchar_t wc = 0;
    size_t r1 = mbrtowc(&wc, "\xED\x95", 2, &st);
    printf("  first call (2 bytes): %s\n", explain(r1));
    size_t r2 = mbrtowc(&wc, "\x9C", 1, &st);
    printf("  second call (1 byte): %s -> U+%04lX\n", explain(r2), (unsigned long)wc);
    puts("  mbstate_t remembers how far it got, so the pieces join up.");
    return 0;
}
