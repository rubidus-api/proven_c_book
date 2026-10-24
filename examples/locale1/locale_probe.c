/* 로케일은 프로그램 전역 상태다 — 무엇으로 시작하고, 무엇으로 바뀌는가. */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* setlocale 의 반환값은 "지금 설정된 로케일의 이름"이다.
   두 번째 인자를 NULL 로 주면 바꾸지 않고 묻기만 한다. */
static void show_current(const char *when)
{
    printf("%s LC_ALL=%s\n", when, setlocale(LC_ALL, NULL));
}

int main(void)
{
    /* ① 프로그램은 언제나 "C" 로케일로 시작한다 (표준 §7.11.1.1p4) */
    show_current("right at start: ");

    /* ② ""(빈 문자열)은 "환경이 정한 로케일"이라는 뜻이다.
          LC_ALL > LC_xxx > LANG 순으로 환경 변수를 본다. */
    const char *applied = setlocale(LC_ALL, "");
    if (applied)
        printf("setlocale(LC_ALL,\"\"): %s\n", applied);
    else
        puts("setlocale(LC_ALL,\"\") failed - the environment has no locale");

    /* ③ 한 글자를 담는 데 필요한 최대 바이트 수는 로케일에 달렸다.
          MB_CUR_MAX 는 매크로처럼 보이지만 *지금 로케일*을 보는 값이다. */
    printf("MB_CUR_MAX:            %zu\n", (size_t)MB_CUR_MAX);

    /* ④ 요청한 로케일이 이 기계에 없으면 setlocale 은 널을 돌려준다.
          그래서 반환값을 반드시 본다 — 조용히 실패하는 자리다. */
    static const char *candidates[] = {
        "C", "C.UTF-8", "en_US.UTF-8", "ko_KR.UTF-8", "ko_KR.EUC-KR",
        "de_DE.UTF-8", "tr_TR.UTF-8", "ja_JP.UTF-8", "no_SUCH.locale",
    };

    puts("\navailable on this machine:");
    char saved[128];
    snprintf(saved, sizeof saved, "%s", setlocale(LC_ALL, NULL));

    for (size_t i = 0; i < sizeof candidates / sizeof *candidates; i++) {
        const char *got = setlocale(LC_ALL, candidates[i]);
        if (got) printf("  %-15s yes  MB_CUR_MAX=%zu\n",
                        candidates[i], (size_t)MB_CUR_MAX);
        else     printf("  %-15s no\n", candidates[i]);
    }

    /* ⑤ 원래대로 되돌린다 — 이름 문자열을 그대로 다시 넘기면 된다.
          단, setlocale 이 돌려준 포인터는 다음 호출에 덮어써질 수 있으므로
          복사해 두어야 한다. */
    setlocale(LC_ALL, saved);
    show_current("after restoring:");

    /* ⑥ 범주는 따로 바꿀 수 있다. 실무의 관용구가 이것이다 —
          사람에게 보일 것은 환경대로, 기계가 읽을 숫자는 "C" 로. */
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    printf("a mixed setting - LC_CTYPE=%s, LC_NUMERIC=%s\n",
           setlocale(LC_CTYPE, NULL), setlocale(LC_NUMERIC, NULL));
    /* LC_ALL 로 물으면 범주가 하나라도 다를 때 전체가 세미콜론으로 나열된다 */
    printf("asking through LC_ALL: %.60s...\n", setlocale(LC_ALL, NULL));
    return 0;
}
