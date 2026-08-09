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
    if (h == SIG_DFL) return "SIG_DFL (default handling)";
    if (h == SIG_IGN) return "SIG_IGN (ignored)";
    if (h == SIG_ERR) return "SIG_ERR (installation failed)";
    return "our own handler";
}

int main(void)
{
    /* ── ① 설치 — 돌려주는 값은 *이전* 처리 방식이다 ───────────── */
    void (*prev)(int) = signal(SIGINT, on_signal);
    printf("return value of signal(SIGINT, on_signal) = %s\n", disposition(prev));

    /* ── ② raise — 자기 자신에게 신호를 보낸다 ─────────────────── */
    printf("before raise(SIGINT): got_int = %d\n", (int)got_int);
    int r = raise(SIGINT);
    printf("raise returned %d (0 means success), got_int = %d\n", r, (int)got_int);

    /* ── ③ 무시 — SIG_IGN 을 걸면 신호가 사라진다 ──────────────── */
    (void)signal(SIGTERM, SIG_IGN);
    (void)raise(SIGTERM);
    printf("raise(SIGTERM) after SIG_IGN: got_term = %d (still 0)\n",
           (int)got_term);

    /* ── ④ 처리 뒤에 처리기가 남는가? — 구현 정의다(표준 §7.14.1.1) ── */
    void (*was)(int) = signal(SIGINT, prev);
    printf("\nafter handling one signal, what is installed: %s\n", disposition(was));
    printf("  -> this implementation resets to SIG_DFL right after handling (the old System V way).\n");
    printf("  -> the standard allows either, so portable code either reinstalls inside\n");
    printf("     the handler or uses POSIX sigaction.\n");

    /* ── ⑤ 표준이 정한 신호는 여섯 개뿐 ───────────────────────── */
    struct { int num; const char *name; const char *meaning; } table[] = {
        { SIGABRT, "SIGABRT", "abnormal termination (abort)" },
        { SIGFPE,  "SIGFPE",  "arithmetic error (division by zero and such)" },
        { SIGILL,  "SIGILL",  "invalid instruction" },
        { SIGINT,  "SIGINT",  "interactive attention signal (Ctrl+C)" },
        { SIGSEGV, "SIGSEGV", "invalid memory access" },
        { SIGTERM, "SIGTERM", "termination request" },
    };
    printf("\nthe %zu signals the standard defines:\n", sizeof table / sizeof *table);
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        printf("  %-8s = %2d   %s\n", table[i].name, table[i].num, table[i].meaning);

    printf("\nsizeof(sig_atomic_t) = %zu bytes\n", sizeof(sig_atomic_t));
    return 0;
}
