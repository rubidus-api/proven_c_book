/* POSIX sigaction — filling the gaps the standard signal left. */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t hits;
static volatile sig_atomic_t last_code;
static volatile sig_atomic_t from_self;

/* With SA_SIGINFO the handler takes three arguments — who sent it, and why */
static void on_signal(int sig, siginfo_t *info, void *ctx)
{
    (void)sig; (void)ctx;
    hits++;
    last_code = info->si_code;
    from_self = (info->si_pid == getpid());   /* the value itself is not printed */
}

static const char *code_name(int code)
{
    switch (code) {
    case SI_USER:   return "SI_USER (sent by kill/raise)";
    case SI_QUEUE:  return "SI_QUEUE (sigqueue)";
    case SI_TIMER:  return "SI_TIMER (a timer)";
    case SI_KERNEL: return "SI_KERNEL (sent by the kernel)";
    case -6:        return "SI_TKILL (raised by the same thread, Linux)";
    default:        return "some other implementation-defined value";
    }
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);          /* start the struct wholly zeroed */
    sa.sa_sigaction = on_signal;        /* with SA_SIGINFO, fill this one */
    sigemptyset(&sa.sa_mask);           /* extra signals blocked while handling */
    sigaddset(&sa.sa_mask, SIGUSR2);    /* defer USR2 while handling USR1 */
    sa.sa_flags = SA_SIGINFO | SA_RESTART;  /* extra info + restart syscalls */

    struct sigaction old;
    if (sigaction(SIGUSR1, &sa, &old) != 0) return 1;

    printf("SIGUSR1 installed with sigaction. Previous handler: %s\n",
           old.sa_handler == SIG_DFL ? "SIG_DFL" : "present");

    /* raise it three times — unlike signal(), the handler *stays* */
    for (int i = 0; i < 3; i++) (void)raise(SIGUSR1);
    printf("after three raises, handled = %d (the handler persists)\n", (int)hits);
    printf("  was the sender ourselves? %s\n", from_self ? "yes" : "no");
    printf("  si_code = %d -> %s\n", (int)last_code, code_name((int)last_code));

    /* -- blocking a signal for a while ---------------------------------- */
    sigset_t block, prev;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &block, &prev) != 0) return 1;

    int before = (int)hits;
    (void)raise(SIGUSR1);               /* blocked, so it becomes pending */
    printf("\nraised while blocked: handled %d -> %d (not delivered yet)\n",
           before, (int)hits);

    sigset_t pending;
    sigpending(&pending);
    printf("  is it pending? %s\n",
           sigismember(&pending, SIGUSR1) ? "yes" : "no");

    sigprocmask(SIG_SETMASK, &prev, nullptr);   /* unblock: delivered right here */
    printf("  handled just after unblocking = %d\n", (int)hits);

    /* -- a look inside the structures ----------------------------------- */
    printf("\nsizeof(struct sigaction) = %zu bytes, sigset_t = %zu bytes\n",
           sizeof(struct sigaction), sizeof(sigset_t));
    printf("SA_RESTART=0x%x, SA_SIGINFO=0x%x, SA_NOCLDWAIT=0x%x\n",
           (unsigned)SA_RESTART, (unsigned)SA_SIGINFO, (unsigned)SA_NOCLDWAIT);
    return 0;
}
