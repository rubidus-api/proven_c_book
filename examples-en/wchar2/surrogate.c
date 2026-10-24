/* The limit of UTF-16 — surrogate pairs, and why "length" has three answers. */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

/* Split one code point into UTF-16 units.
   The BMP (U+0000..U+FFFF, minus D800..DFFF) is one unit as it stands;
   above it (U+10000..U+10FFFF) it takes two — a surrogate pair. */
static int to_utf16(unsigned long cp, char16_t out[2])
{
    if (cp > 0x10FFFFUL) return 0;                 /* outside Unicode */
    if (cp >= 0xD800UL && cp <= 0xDFFFUL) return 0; /* surrogates are not characters */
    if (cp < 0x10000UL) { out[0] = (char16_t)cp; return 1; }

    unsigned long v = cp - 0x10000UL;              /* down to 20 bits */
    out[0] = (char16_t)(0xD800UL + (v >> 10));     /* top 10 bits -> high surrogate */
    out[1] = (char16_t)(0xDC00UL + (v & 0x3FFUL)); /* low 10 bits -> low surrogate */
    return 2;
}

/* The other way — a pair back into a code point */
static unsigned long from_pair(char16_t hi, char16_t lo)
{
    return 0x10000UL + (((unsigned long)hi - 0xD800UL) << 10)
                     +  ((unsigned long)lo - 0xDC00UL);
}

static void report(const char *name, unsigned long cp)
{
    char16_t u16[2] = { 0, 0 };
    int n = to_utf16(cp, u16);

    char cpname[16];
    snprintf(cpname, sizeof cpname, "U+%04lX", cp);
    printf("  %-9s -> ", cpname);
    if (n == 0)      printf("cannot be represented   ");
    else if (n == 1) printf("%04X        one unit        ", u16[0]);
    else             printf("%04X %04X   back to U+%04lX", u16[0], u16[1],
                             from_pair(u16[0], u16[1]));
    printf("  %s\n", name);
}

/* Measure one string three ways */
static void lengths(const char *label, const char *utf8)
{
    size_t bytes = strlen(utf8);

    /* Count code points and UTF-16 units by hand */
    size_t cps = 0, u16units = 0;
    mbstate_t st;
    memset(&st, 0, sizeof st);
    for (size_t pos = 0; pos < bytes; ) {
        wchar_t wc = 0;
        size_t r = mbrtowc(&wc, utf8 + pos, bytes - pos, &st);
        if (r == (size_t)-1 || r == (size_t)-2) break;
        if (r == 0) r = 1;
        pos += r;
        cps++;
        char16_t tmp[2];
        u16units += (size_t)to_utf16((unsigned long)wc, tmp);
    }
    printf("  UTF-8 %2zu bytes  %zu code point%s  %zu UTF-16 unit%s   %s\n",
           bytes, cps, cps == 1 ? " " : "s", u16units,
           u16units == 1 ? " " : "s", label);
}

int main(void)
{
    puts("[code point -> UTF-16]");
    report("'A'", 0x41);
    report("'한' (Hangul)", 0xD55C);
    report("U+FFFD replacement char", 0xFFFD);
    report("a surrogate itself, D800", 0xD800);
    report("emoji U+1F600", 0x1F600);
    report("CJK extension U+2A6B2", 0x2A6B2);
    report("the last code point", 0x10FFFF);
    report("beyond the range", 0x110000);

    puts("\nThe arithmetic, for U+1F600:");
    unsigned long v = 0x1F600UL - 0x10000UL;
    printf("  0x1F600 - 0x10000 = 0x%05lX (20 bits)\n", v);
    printf("  top 10 bits 0x%03lX + 0xD800 = 0x%04lX\n", v >> 10, 0xD800UL + (v >> 10));
    printf("  low 10 bits 0x%03lX + 0xDC00 = 0x%04lX\n",
           v & 0x3FF, 0xDC00UL + (v & 0x3FF));

    const char *loc = setlocale(LC_CTYPE, "C.UTF-8");
    if (!loc) loc = setlocale(LC_CTYPE, "en_US.UTF-8");
    if (!loc) { puts("\nno UTF-8 locale — skipping the length demonstration"); return 0; }

    puts("\n[three lengths of the same string]");
    lengths("\"Hi\"", "Hi");
    lengths("\"한글\" (two Hangul syllables)", "한글");
    lengths("one emoji", "\xF0\x9F\x98\x80");
    lengths("a mixture", "a한\xF0\x9F\x98\x80");
    printf("  wchar_t here: %zu bytes -> UTF-32. On Windows: 2 bytes -> UTF-16.\n",
           sizeof(wchar_t));
    return 0;
}
