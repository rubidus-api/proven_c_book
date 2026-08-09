/* '글자 수'는 세 가지다 — 바이트, 코드포인트, 그리고 사람이 보는 글자. */
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* 사람이 보는 한 글자(그래핌 클러스터)의 완전한 규칙은 유니코드 UAX #29 에
   있고, 제대로 구현하려면 표가 필요하다. 여기서는 자주 만나는 네 가지만
   추린 *간이 규칙*으로 센다 — 개념을 눈으로 보는 것이 목적이다.
     ① 결합 표시(U+0300~U+036F 등)는 앞 글자에 붙는다
     ② 한글 조합 자모(U+1100~U+11FF)는 앞 글자에 붙는다
     ③ ZWJ(U+200D) 다음 글자는 앞 글자에 붙는다
     ④ 지역 표시자(U+1F1E6~U+1F1FF)는 둘이 하나가 된다(국기) */
static int is_combining(unsigned long c)
{
    return (c >= 0x0300 && c <= 0x036F)     /* 결합 발음 기호 */
        || (c >= 0x1160 && c <= 0x11FF)     /* 한글 조합 중성·종성 */
        || (c >= 0xFE00 && c <= 0xFE0F)     /* 변이 선택자 */
        || (c >= 0x1F3FB && c <= 0x1F3FF);  /* 피부색 수정자 */
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
    if (!loc) { puts("no UTF-8 locale - skipping this demonstration"); return 0; }

    puts("[the same letter can be written two ways]");
    measure("\"가\" precomposed U+AC00",            "\uAC00");
    measure("\"가\" decomposed U+1100 U+1161",     "\u1100\u1161");
    measure("\"e\\u0301\" a combining accent",        "e\u0301");
    measure("\"\\u00E9\" a precomposed letter",    "\u00E9");

    puts("\n[an emoji can be several code points that look like one character]");
    measure("a smiling face",              "\U0001F600");
    measure("a family (joined with ZWJ)", "\U0001F468\u200D\U0001F469\u200D\U0001F467");
    measure("a flag (two regional indicators)", "\U0001F1F0\U0001F1F7");
    measure("a waving hand + skin tone",     "\U0001F44B\U0001F3FD");

    puts("\n[one sentence]");
    measure("\"Hello, 세계!\"", "Hello, 세계!");

    puts("\nin short: before asking for a 'character count', decide *which layer* you are counting.");
    puts("  bytes = the unit of storage and transmission,  code points = the unit of Unicode,");
    puts("  grapheme clusters = what the cursor steps over at once (UAX #29).");
    puts("the count above is a simplified version using four common rules - a complete decision");
    puts("belongs to a dedicated library such as ICU.");
    return 0;
}
