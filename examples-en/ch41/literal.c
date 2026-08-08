/* What a string literal really is — an array, adjacent ones join, and the prefixes. */
#include <stdio.h>
#include <inttypes.h>   /* PRIu64 — a standard macro built on the joining rule */
#include <wchar.h>

/* the idiom of giving a format fragment a name — it joins in the printf below */
#define TEMP_FMT  "%.1f"
#define ID_FMT    "%d"

/* assembling a version string — the value is written in one place only */
#define MY_PROGRAM_VERSION  "v3.1.2"
#define PROGRAM_TITLE       "my_program " MY_PROGRAM_VERSION

/* a version kept as numbers becomes a string through a double indirection (chapter 56) */
#define VER_MAJOR 3
#define VER_MINOR 1
#define STR_RAW(x) #x
#define STR(x)     STR_RAW(x)
#define VERSION_FROM_NUMBERS  "v" STR(VER_MAJOR) "." STR(VER_MINOR)

int main(void)
{
    /* ── (1) a literal is an array ───────────────────────────── */
    printf("sizeof \"abcdef\" = %zu  (six characters + NUL)\n", sizeof "abcdef");
    printf("sizeof \"\"       = %zu  (an empty string is still one NUL)\n", sizeof "");

    /* being an array, it takes a subscript — chapter 38's a[i] == *(a+i) as it is */
    printf("\"abcdef\"[3] = %c\n", "abcdef"[3]);
    printf("3[\"abcdef\"] = %c   <- the subscript commutes (it means the same)\n", 3["abcdef"]);

    /* ── (2) adjacent literals join into one ─────────────────── */
    printf("%s\n", "abc" "def");                 /* -> "abcdef" */
    printf("sizeof(\"abc\" \"def\") = %zu\n", sizeof("abc" "def"));

    /* writing a long message across lines — no backslashes needed */
    const char *help =
        "usage: tool [options] file\n"
        "  -v   verbose\n"
        "  -o   output file\n";
    printf("%s", help);

    /* ── (3) naming the fragments and assembling them ────────── */
    puts(PROGRAM_TITLE);
    printf("assembled from numbers: %s\n", VERSION_FROM_NUMBERS);

    int    id   = 7;
    double temp = 36.5;
    printf("id = " ID_FMT ", temp = " TEMP_FMT "\n", id, temp);

    /* the standard library uses the same technique — format macros for fixed-width integers */
    uint64_t total = 1234567890123ULL;
    printf("total = %" PRIu64 "\n", total);

    /* ── (4) prefixes — the letter that settles the encoding ─── */
    printf("sizeof \"AB\"  = %zu (char)\n",    sizeof "AB");
    printf("sizeof u8\"AB\" = %zu (UTF-8)\n",  sizeof u8"AB");
    printf("sizeof L\"AB\"  = %zu (wchar_t is %zu bytes)\n",
           sizeof L"AB", sizeof(wchar_t));
    printf("sizeof u\"AB\"  = %zu (UTF-16)\n", sizeof u"AB");
    printf("sizeof U\"AB\"  = %zu (UTF-32)\n", sizeof U"AB");

    /* join one with a prefix and one without and the prefixed one wins */
    const wchar_t *w = L"wide" " and narrow";
    printf("L\"wide\" \" and narrow\" -> length %zu (it becomes a wide string)\n",
           wcslen(w));
    return 0;
}
