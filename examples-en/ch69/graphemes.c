/* "How many characters" has three answers — bytes, code points, and what a reader sees. */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* The full rule for "one character as a reader sees it" (a grapheme cluster)
   is Unicode UAX #29, and a proper implementation needs tables. Here we count
   with a *simplified rule* covering the four cases you meet most — the point is
   to see the concept, not to be complete.
     (1) combining marks (U+0300..U+036F and friends) join the previous one
     (2) Hangul conjoining jamo (U+1100..U+11FF) join the previous one
     (3) whatever follows a ZWJ (U+200D) joins the previous one
     (4) regional indicators (U+1F1E6..U+1F1FF) pair up into a flag */
static int is_combining(unsigned long c)
{
    return (c >= 0x0300 && c <= 0x036F)     /* combining diacritics */
        || (c >= 0x1160 && c <= 0x11FF)     /* Hangul vowels and final consonants */
        || (c >= 0xFE00 && c <= 0xFE0F)     /* variation selectors */
        || (c >= 0x1F3FB && c <= 0x1F3FF);  /* skin-tone modifiers */
}
static int is_regional(unsigned long c) { return c >= 0x1F1E6 && c <= 0x1F1FF; }

static void measure(const char *label, const char *s)
{
    size_t bytes = strlen(s);
    size_t cps = 0, clusters = 0;

    mbstate_t st;
    memset(&st, 0, sizeof st);

    unsigned long prev = 0;
    int prev_was_zwj = 0, prev_regional = 0;

    for (size_t pos = 0; pos < bytes; ) {
        wchar_t wc = 0;
        size_t r = mbrtowc(&wc, s + pos, bytes - pos, &st);
        if (r == (size_t)-1 || r == (size_t)-2) break;
        if (r == 0) r = 1;
        pos += r;
        cps++;

        unsigned long c = (unsigned long)wc;
        int joins = (cps > 1) && (is_combining(c) || prev_was_zwj || c == 0x200D
                                  || (is_regional(c) && prev_regional));
        if (!joins) clusters++;

        prev_was_zwj = (c == 0x200D);
        prev_regional = is_regional(c) && !prev_regional;
        prev = c;
    }
    (void)prev;

    printf("  bytes %2zu  code points %2zu  visible characters %2zu   %s\n",
           bytes, cps, clusters, label);
}

int main(void)
{
    const char *loc = setlocale(LC_CTYPE, "C.UTF-8");
    if (!loc) loc = setlocale(LC_CTYPE, "en_US.UTF-8");
    if (!loc) { puts("no UTF-8 locale — skipping this demonstration"); return 0; }

    puts("[the same letter, written two ways]");
    measure("\"가\" precomposed, U+AC00",       "\uAC00");
    measure("\"가\" composed, U+1100 U+1161",  "\u1100\u1161");
    measure("\"e\\u0301\", combining accent",  "e\u0301");
    measure("\"\\u00E9\", precomposed",         "\u00E9");

    puts("\n[emoji: several code points that look like one character]");
    measure("grinning face",          "\U0001F600");
    measure("family, joined by ZWJ",  "\U0001F468\u200D\U0001F469\u200D\U0001F467");
    measure("a flag, two indicators", "\U0001F1F0\U0001F1F7");
    measure("wave + skin tone",       "\U0001F44B\U0001F3FD");

    puts("\n[one sentence]");
    measure("\"Hello, 세계!\"", "Hello, 세계!");

    puts("\nSo: before asking \"how many characters\", settle *which layer*.");
    puts("  bytes = what you store and send,  code points = Unicode's unit,");
    puts("  grapheme clusters = what the cursor steps over (UAX #29).");
    puts("The count above uses four simplified rules — a complete decision");
    puts("belongs to a dedicated library such as ICU.");
    return 0;
}
