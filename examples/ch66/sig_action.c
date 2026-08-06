/* POSIX sigaction — 표준 signal 이 남긴 빈틈을 메운다. */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t hits;
static volatile sig_atomic_t last_code;
static volatile sig_atomic_t from_self;

/* SA_SIGINFO 를 주면 처리기가 세 인자를 받는다 — 누가 왜 보냈는지가 들어온다 */
static void on_signal(int sig, siginfo_t *info, void *ctx)
{
    (void)sig; (void)ctx;
    hits++;
    last_code = info->si_code;
    from_self = (info->si_pid == getpid());   /* 값 자체는 인쇄하지 않는다 */
}

static const char *code_name(int code)
{
    switch (code) {
    case SI_USER:   return "SI_USER(kill/raise 로 사람이 보냄)";
    case SI_QUEUE:  return "SI_QUEUE(sigqueue)";
    case SI_TIMER:  return "SI_TIMER(타이머)";
    case SI_KERNEL: return "SI_KERNEL(커널이 보냄)";
    case -6:        return "SI_TKILL(같은 스레드가 raise 로 보냄, 리눅스)";
    default:        return "구현이 정한 그 밖의 값";
    }
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);          /* 구조체는 통째로 0 으로 시작한다 */
    sa.sa_sigaction = on_signal;        /* SA_SIGINFO 를 쓰면 이쪽을 채운다 */
    sigemptyset(&sa.sa_mask);           /* 처리 중에 추가로 막을 신호들 */
    sigaddset(&sa.sa_mask, SIGUSR2);    /* USR1 처리 중에는 USR2 를 미룬다 */
    sa.sa_flags = SA_SIGINFO | SA_RESTART;  /* 정보 받기 + 시스템 호출 재시작 */

    struct sigaction old;
    if (sigaction(SIGUSR1, &sa, &old) != 0) return 1;

    printf("sigaction 으로 SIGUSR1 설치. 이전 처리기: %s\n",
           old.sa_handler == SIG_DFL ? "SIG_DFL" : "있음");

    /* 세 번 보낸다 — signal() 과 달리 처리기가 *남아 있다* */
    for (int i = 0; i < 3; i++) (void)raise(SIGUSR1);
    printf("세 번 raise 한 뒤 처리 횟수 = %d (처리기가 유지된다)\n", (int)hits);
    printf("  보낸 이가 자기 자신인가? %s\n", from_self ? "그렇다" : "아니다");
    printf("  si_code = %d → %s\n", (int)last_code, code_name((int)last_code));

    /* ── 신호를 잠시 막아 두기(블록) ─────────────────────────── */
    sigset_t block, prev;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &block, &prev) != 0) return 1;

    int before = (int)hits;
    (void)raise(SIGUSR1);               /* 막혀 있으므로 대기 상태가 된다 */
    printf("\n막아 둔 채 raise: 처리 횟수 %d → %d (아직 처리되지 않음)\n",
           before, (int)hits);

    sigset_t pending;
    sigpending(&pending);
    printf("  대기 중인가? %s\n",
           sigismember(&pending, SIGUSR1) ? "그렇다" : "아니다");

    sigprocmask(SIG_SETMASK, &prev, nullptr);   /* 풀면 그 자리에서 배달된다 */
    printf("  막기를 푼 직후 처리 횟수 = %d\n", (int)hits);

    /* ── 구조체 안을 들여다보기 ──────────────────────────────── */
    printf("\nstruct sigaction 의 크기 = %zu바이트, sigset_t = %zu바이트\n",
           sizeof(struct sigaction), sizeof(sigset_t));
    printf("SA_RESTART=0x%x, SA_SIGINFO=0x%x, SA_NOCLDWAIT=0x%x\n",
           (unsigned)SA_RESTART, (unsigned)SA_SIGINFO, (unsigned)SA_NOCLDWAIT);
    return 0;
}
