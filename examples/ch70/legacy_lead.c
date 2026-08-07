/* 레거시 2바이트 인코딩 — 후행 바이트가 아스키와 겹칠 때 무슨 일이 나는가. */
#include <stdio.h>
#include <string.h>

/* 인코딩마다 '선행 바이트'와 '후행 바이트'의 범위가 다르다.
   위험한 것은 후행 바이트 범위가 아스키 영역과 겹치는 인코딩이다. */
static void show_table(void)
{
    puts("인코딩별 바이트 범위 (2바이트 문자)");
    puts("  인코딩      선행 바이트        후행 바이트           0x5C('\\') 가 후행에?");
    puts("  EUC-KR      A1-FE              A1-FE                 아니다");
    puts("  CP949(UHC)  81-FE              41-5A,61-7A,81-FE     아니다");
    puts("  Shift_JIS   81-9F,E0-FC        40-7E,80-FC           ★ 그렇다");
    puts("  Big5        81-FE              40-7E,A1-FE           ★ 그렇다");
    puts("  GBK         81-FE              40-FE(7F 제외)        ★ 그렇다");
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
    dump("바이트", path);

    /* 흔히 쓰는 방법 — 바이트만 보고 마지막 '\' 를 찾는다 */
    const char *hit = strrchr((const char *)path, 0x5C);
    long naive = hit ? (long)(hit - (const char *)path) : -1;
    long safe  = safe_last_sep(path, is_lead);

    printf("  strrchr 로 찾은 마지막 '\\': 위치 %ld\n", naive);
    printf("  선행 바이트를 아는 훑기:     위치 %ld\n", safe);
    if (naive == safe) puts("  → 같다. 이 인코딩에서는 안전하다.");
    else {
        puts("  → 다르다! strrchr 이 문자의 *후행 바이트*를 구분자로 잘못 짚었다.");
        printf("     그 자리에서 자르면 파일 이름이 \"");
        for (size_t i = (size_t)naive + 1; path[i]; i++) printf("%02X ", path[i]);
        puts("\" 라는 깨진 조각이 된다.");
    }
}

int main(void)
{
    show_table();
    test("Shift_JIS 의 C:\\表\\ソ.txt", sjis_path, sjis_is_lead);
    test("EUC-KR 의 C:\\한글.txt", euckr_path, euckr_is_lead);

    puts("\n이 사고는 유명한 이름까지 얻었다 — 일본에서는 '다메모지'(ダメ文字),");
    puts("대표적으로 表·ソ·十·ダ 같은 글자가 걸린다. UTF-8 에는 이 문제가 없다:");
    puts("후행 바이트가 언제나 0x80 이상이라 아스키와 겹치지 않기 때문이다.");

    const unsigned char utf8_path[] = "C:\\한글.txt";
    dump("UTF-8", utf8_path);
    const char *h = strrchr((const char *)utf8_path, 0x5C);
    printf("  strrchr 로 찾은 마지막 '\\': 위치 %ld — 언제나 옳다\n",
           h ? (long)(h - (const char *)utf8_path) : -1);
    return 0;
}
