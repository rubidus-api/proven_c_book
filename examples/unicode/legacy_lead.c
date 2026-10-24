/* 레거시 2바이트 인코딩 — 후행 바이트가 아스키와 겹칠 때 무슨 일이 나는가. */
#include <stdio.h>
#include <string.h>

/* 인코딩마다 '선행 바이트'와 '후행 바이트'의 범위가 다르다.
   위험한 것은 후행 바이트 범위가 아스키 영역과 겹치는 인코딩이다. */
static void show_table(void)
{
    puts("byte ranges per encoding (two-byte characters)");
    puts("  encoding    lead byte          trail byte            can 0x5C('\\') be a trail?");
    puts("  EUC-KR      A1-FE              A1-FE                 no");
    puts("  CP949(UHC)  81-FE              41-5A,61-7A,81-FE     no");
    puts("  Shift_JIS   81-9F,E0-FC        40-7E,80-FC           * yes");
    puts("  Big5        81-FE              40-7E,A1-FE           * yes");
    puts("  GBK         81-FE              40-FE (except 7F)     * yes");
}

/* Shift_JIS 로 적은 경로: C:\表\ソ.txt
   表 = 95 5C, ソ = 83 5C — 둘 다 후행 바이트가 0x5C 다(iconv 로 확인). */
static const unsigned char sjis_path[] = {
    'C', ':', 0x5C, 0x95, 0x5C, 0x5C, 0x83, 0x5C, '.', 't', 'x', 't', 0
};

/* EUC-KR 로 적은 경로: C:\한글.txt  (한 = C7 D1, 글 = B1 DB) */
static const unsigned char euckr_path[] = {
    'C', ':', 0x5C, 0xC7, 0xD1, 0xB1, 0xDB, '.', 't', 'x', 't', 0
};

static int sjis_is_lead(unsigned char c)
{ return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC); }

static int euckr_is_lead(unsigned char c) { return c >= 0xA1 && c <= 0xFE; }

/* 선행 바이트를 건너뛰며 마지막 구분자를 찾는다 — 올바른 방법 */
static long safe_last_sep(const unsigned char *s, int (*is_lead)(unsigned char))
{
    long last = -1;
    for (size_t i = 0; s[i]; ) {
        if (is_lead(s[i]) && s[i + 1]) { i += 2; continue; }  /* 두 바이트 문자 */
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

    /* 흔히 쓰는 방법 — 바이트만 보고 마지막 '\' 를 찾는다 */
    const char *hit = strrchr((const char *)path, 0x5C);
    long naive = hit ? (long)(hit - (const char *)path) : -1;
    long safe  = safe_last_sep(path, is_lead);

    printf("  last '\\' found by strrchr:     position %ld\n", naive);
    printf("  a scan that knows lead bytes:  position %ld\n", safe);
    if (naive == safe) puts("  -> the same. Safe in this encoding.");
    else {
        puts("  -> different! strrchr mistook the *trail byte* of a character for the separator.");
        printf("     cutting there leaves the file name as the broken fragment \"");
        for (size_t i = (size_t)naive + 1; path[i]; i++) printf("%02X ", path[i]);
        puts("\".");
    }
}

int main(void)
{
    show_table();
    test("Shift_JIS: C:\\表\\ソ.txt", sjis_path, sjis_is_lead);
    test("EUC-KR: C:\\한글.txt", euckr_path, euckr_is_lead);

    puts("\nthis accident is famous enough to have a name - in Japan, 'dame-moji' (ダメ文字),");
    puts("typically letters such as 表, ソ, 十 and ダ. UTF-8 does not have the problem:");
    puts("its trail bytes are always 0x80 or above, so they never collide with ASCII.");

    const unsigned char utf8_path[] = "C:\\한글.txt";
    dump("UTF-8", utf8_path);
    const char *h = strrchr((const char *)utf8_path, 0x5C);
    printf("  last '\\' found by strrchr:     position %ld - always right\n",
           h ? (long)(h - (const char *)utf8_path) : -1);
    return 0;
}
