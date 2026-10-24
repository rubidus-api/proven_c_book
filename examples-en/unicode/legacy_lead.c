/* Legacy two-byte encodings — what happens when trail bytes overlap ASCII. */
#include <stdio.h>
#include <string.h>

/* Each encoding has its own lead-byte and trail-byte ranges.
   The dangerous ones are those whose trail bytes reach into ASCII. */
static void show_table(void)
{
    puts("byte ranges per encoding (two-byte characters)");
    puts("  encoding    lead bytes         trail bytes           0x5C('\\') as trail?");
    puts("  EUC-KR      A1-FE              A1-FE                 no");
    puts("  CP949(UHC)  81-FE              41-5A,61-7A,81-FE     no");
    puts("  Shift_JIS   81-9F,E0-FC        40-7E,80-FC           * yes");
    puts("  Big5        81-FE              40-7E,A1-FE           * yes");
    puts("  GBK         81-FE              40-FE(not 7F)         * yes");
}

/* A path written in Shift_JIS: C:\表\ソ.txt
   表 = 95 5C, ソ = 83 5C — both have 0x5C as their trail byte (checked with iconv). */
static const unsigned char sjis_path[] = {
    'C', ':', 0x5C, 0x95, 0x5C, 0x5C, 0x83, 0x5C, '.', 't', 'x', 't', 0
};

/* The same in EUC-KR: C:\한글.txt  (한 = C7 D1, 글 = B1 DB) */
static const unsigned char euckr_path[] = {
    'C', ':', 0x5C, 0xC7, 0xD1, 0xB1, 0xDB, '.', 't', 'x', 't', 0
};

static int sjis_is_lead(unsigned char c)
{ return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC); }

static int euckr_is_lead(unsigned char c) { return c >= 0xA1 && c <= 0xFE; }

/* Find the last separator, stepping over lead bytes — the correct way */
static long safe_last_sep(const unsigned char *s, int (*is_lead)(unsigned char))
{
    long last = -1;
    for (size_t i = 0; s[i]; ) {
        if (is_lead(s[i]) && s[i + 1]) { i += 2; continue; }  /* a two-byte character */
        if (s[i] == 0x5C) last = (long)i;
        i += 1;
    }
    return last;
}

static void dump(const char *label, const unsigned char *s)
{
    printf("  %-10s", label);
    for (size_t i = 0; s[i]; i++) printf(" %02X", s[i]);
    puts("");
}

static void test(const char *name, const unsigned char *path,
                 int (*is_lead)(unsigned char))
{
    printf("\n[%s]\n", name);
    dump("bytes", path);

    /* The usual way — look at bytes only, find the last '\\' */
    const char *hit = strrchr((const char *)path, 0x5C);
    long naive = hit ? (long)(hit - (const char *)path) : -1;
    long safe  = safe_last_sep(path, is_lead);

    printf("  last '\\' found by strrchr:   at %ld\n", naive);
    printf("  a lead-byte-aware scan:      at %ld\n", safe);
    if (naive == safe) puts("  -> the same. This encoding is safe here.");
    else {
        puts("  -> different! strrchr took a *trail byte* for a separator.");
        printf("     Cut there and the file name becomes \"");
        for (size_t i = (size_t)naive + 1; path[i]; i++) printf("%02X ", path[i]);
        puts("\" — a broken fragment.");
    }
}

int main(void)
{
    show_table();
    test("Shift_JIS: C:\\表\\ソ.txt", sjis_path, sjis_is_lead);
    test("EUC-KR: C:\\한글.txt", euckr_path, euckr_is_lead);

    puts("\nThis accident even has a name in Japan — dame-moji (ダメ文字),");
    puts("with 表, ソ, 十 and ダ as the usual victims. UTF-8 does not have it:");
    puts("its trail bytes are always 0x80 or above, so they never touch ASCII.");

    const unsigned char utf8_path[] = "C:\\한글.txt";
    dump("UTF-8", utf8_path);
    const char *h = strrchr((const char *)utf8_path, 0x5C);
    printf("  last '\\' found by strrchr:   at %ld — always right\n",
           h ? (long)(h - (const char *)utf8_path) : -1);
    return 0;
}
