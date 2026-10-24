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
    case SI_USER:   return "SI_USER (sent by a person, via kill/raise)";
    case SI_QUEUE:  return "SI_QUEUE(sigqueue)";
    case SI_TIMER:  return "SI_TIMER (a timer)";
    case SI_KERNEL: return "SI_KERNEL (sent by the kernel)";
    case -6:        return "SI_TKILL (raise from the same thread, Linux)";
    default:        return "some other implementation-defined value";
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

    printf("SIGUSR1 installed with sigaction. Previous handler: %s\n",
           old.sa_handler == SIG_DFL ? "SIG_DFL" : "there was one");

    /* 세 번 보낸다 — signal() 과 달리 처리기가 *남아 있다* */
    for (int i = 0; i < 3; i++) (void)raise(SIGUSR1);
    printf("after raising three times, handled = %d (the handler stays installed)\n", (int)hits);
    printf("  was the sender ourselves? %s\n", from_self ? "yes" : "no");
    printf("  si_code = %d → %s\n", (int)last_code, code_name((int)last_code));

    /* ── 신호를 잠시 막아 두기(블록) ─────────────────────────── */
    sigset_t block, prev;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &block, &prev) != 0) return 1;

    int before = (int)hits;
    (void)raise(SIGUSR1);               /* 막혀 있으므로 대기 상태가 된다 */
    printf("\nraise while blocked: handled %d -> %d (not delivered yet)\n",
           before, (int)hits);

    sigset_t pending;
    sigpending(&pending);
    printf("  is it pending? %s\n",
           sigismember(&pending, SIGUSR1) ? "yes" : "no");

    sigprocmask(SIG_SETMASK, &prev, nullptr);   /* 풀면 그 자리에서 배달된다 */
    printf("  right after unblocking, handled = %d\n", (int)hits);

    /* ── 구조체 안을 들여다보기 ──────────────────────────────── */
    printf("\nsizeof(struct sigaction) = %zu bytes, sigset_t = %zu bytes\n",
           sizeof(struct sigaction), sizeof(sigset_t));
    printf("SA_RESTART=0x%x, SA_SIGINFO=0x%x, SA_NOCLDWAIT=0x%x\n",
           (unsigned)SA_RESTART, (unsigned)SA_SIGINFO, (unsigned)SA_NOCLDWAIT);
    return 0;
}
