/* <signal.h> 의 표준 부분 — 세 가지 처리 방식과 raise. */
#include <signal.h>
#include <stdio.h>
#include <string.h>

/* 표준이 허락하는 처리기의 몸통은 이만큼이다:
   volatile sig_atomic_t 에 값을 대입하는 것. 그 밖은 계약 밖이다. */
static volatile sig_atomic_t got_int;
static volatile sig_atomic_t got_term;

static void on_signal(int sig)
{
    if (sig == SIGINT)  got_int  = 1;
    if (sig == SIGTERM) got_term = 1;
    /* 여기서 printf 를 부르지 않는다 — 이유는 본문에서 */
}

static const char *disposition(void (*h)(int))
{
    if (h == SIG_DFL) return "SIG_DFL(기본 처리)";
    if (h == SIG_IGN) return "SIG_IGN(무시)";
    if (h == SIG_ERR) return "SIG_ERR(설정 실패)";
    return "내 처리기";
}

int main(void)
{
    /* ── ① 설치 — 돌려주는 값은 *이전* 처리 방식이다 ───────────── */
    void (*prev)(int) = signal(SIGINT, on_signal);
    printf("signal(SIGINT, on_signal) 의 반환값 = %s\n", disposition(prev));

    /* ── ② raise — 자기 자신에게 신호를 보낸다 ─────────────────── */
    printf("raise(SIGINT) 호출 전: got_int = %d\n", (int)got_int);
    int r = raise(SIGINT);
    printf("raise 반환값 = %d (0 이면 성공), got_int = %d\n", r, (int)got_int);

    /* ── ③ 무시 — SIG_IGN 을 걸면 신호가 사라진다 ──────────────── */
    (void)signal(SIGTERM, SIG_IGN);
    (void)raise(SIGTERM);
    printf("SIG_IGN 을 건 뒤 raise(SIGTERM): got_term = %d (그대로 0)\n",
           (int)got_term);

    /* ── ④ 처리 뒤에 처리기가 남는가? — 구현 정의다(표준 §7.14.1.1) ── */
    void (*was)(int) = signal(SIGINT, prev);
    printf("\n신호를 한 번 처리한 뒤 걸려 있던 것은: %s\n", disposition(was));
    printf("  → 이 구현은 처리 직후 SIG_DFL 로 되돌린다(옛 System V 방식).\n");
    printf("  → 표준은 둘 중 어느 쪽이든 좋다고 말한다 — 그래서 이식성 있는\n");
    printf("     코드는 처리기 안에서 다시 걸거나, POSIX sigaction 을 쓴다.\n");

    /* ── ⑤ 표준이 정한 신호는 여섯 개뿐 ───────────────────────── */
    struct { int num; const char *name; const char *meaning; } table[] = {
        { SIGABRT, "SIGABRT", "비정상 종료(abort)" },
        { SIGFPE,  "SIGFPE",  "산술 오류(0 나눗셈 등)" },
        { SIGILL,  "SIGILL",  "잘못된 명령" },
        { SIGINT,  "SIGINT",  "대화형 주의 신호(Ctrl+C)" },
        { SIGSEGV, "SIGSEGV", "잘못된 기억 접근" },
        { SIGTERM, "SIGTERM", "종료 요청" },
    };
    printf("\n표준이 정한 신호 %zu개:\n", sizeof table / sizeof *table);
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        printf("  %-8s = %2d   %s\n", table[i].name, table[i].num, table[i].meaning);

    printf("\nsig_atomic_t 의 크기 = %zu바이트\n", sizeof(sig_atomic_t));
    return 0;
}
