/* 문자열 리터럴의 정체 — 배열이라는 것, 이어 붙는다는 것, 그리고 접두사. */
#include <stdio.h>
#include <inttypes.h>   /* PRIu64 — 이어 붙이기 규칙 위에 세워진 표준 매크로 */
#include <wchar.h>

/* 서식 조각을 이름으로 두는 관용구 — 아래 printf 에서 이어 붙는다 */
#define TEMP_FMT  "%.1f"
#define ID_FMT    "%d"

/* 버전 문자열 조립 — 값은 한 곳에만 적는다 */
#define MY_PROGRAM_VERSION  "v3.1.2"
#define PROGRAM_TITLE       "my_program " MY_PROGRAM_VERSION

/* 숫자로 관리하는 버전은 두 겹 우회로 문자열이 된다(57장) */
#define VER_MAJOR 3
#define VER_MINOR 1
#define STR_RAW(x) #x
#define STR(x)     STR_RAW(x)
#define VERSION_FROM_NUMBERS  "v" STR(VER_MAJOR) "." STR(VER_MINOR)

int main(void)
{
    /* ── ① 리터럴은 배열이다 ─────────────────────────────────── */
    printf("sizeof \"abcdef\" = %zu  (여섯 글자 + NUL)\n", sizeof "abcdef");
    printf("sizeof \"\"       = %zu  (빈 문자열도 NUL 한 칸)\n", sizeof "");

    /* 배열이므로 첨자를 붙일 수 있다 — 38장의 a[i] == *(a+i) 그대로 */
    printf("\"abcdef\"[3] = %c\n", "abcdef"[3]);
    printf("3[\"abcdef\"] = %c   ← 첨자가 교환된다(같은 뜻이다)\n", 3["abcdef"]);

    /* ── ② 인접한 리터럴은 하나로 이어진다 ───────────────────── */
    printf("%s\n", "abc" "def");                 /* -> "abcdef" */
    printf("sizeof(\"abc\" \"def\") = %zu\n", sizeof("abc" "def"));

    /* 긴 문장을 줄 나눠 적을 때 — 역슬래시 없이도 된다 */
    const char *help =
        "사용법: tool [옵션] 파일\n"
        "  -v   자세히\n"
        "  -o   출력 파일\n";
    printf("%s", help);

    /* ── ③ 조각을 이름으로 두고 조립하기 ─────────────────────── */
    puts(PROGRAM_TITLE);
    printf("숫자에서 조립: %s\n", VERSION_FROM_NUMBERS);

    int    id   = 7;
    double temp = 36.5;
    printf("id = " ID_FMT ", temp = " TEMP_FMT "\n", id, temp);

    /* 표준 라이브러리가 같은 기법을 쓴다 — 고정 폭 정수의 서식 매크로 */
    uint64_t total = 1234567890123ULL;
    printf("total = %" PRIu64 "\n", total);

    /* ── ④ 접두사 — 인코딩을 정하는 글자 ─────────────────────── */
    printf("sizeof \"AB\"  = %zu (char)\n",    sizeof "AB");
    printf("sizeof u8\"AB\" = %zu (UTF-8)\n",  sizeof u8"AB");
    printf("sizeof L\"AB\"  = %zu (wchar_t %zu바이트)\n",
           sizeof L"AB", sizeof(wchar_t));
    printf("sizeof u\"AB\"  = %zu (UTF-16)\n", sizeof u"AB");
    printf("sizeof U\"AB\"  = %zu (UTF-32)\n", sizeof U"AB");

    /* 접두사가 있는 것과 없는 것을 이어 붙이면 있는 쪽을 따른다 */
    const wchar_t *w = L"wide" " and narrow";
    printf("L\"wide\" \" and narrow\" -> 길이 %zu (와이드 문자열이 된다)\n",
           wcslen(w));
    return 0;
}
