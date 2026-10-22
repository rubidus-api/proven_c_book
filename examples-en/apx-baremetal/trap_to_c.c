/* The road from a hardware exception up to a C function --- seen on top of an OS. */
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

static sigjmp_buf escape;
static volatile sig_atomic_t why = 0;
static void *where = NULL;

/* The handler receives what the hardware gives --- a cause number and the faulting address */
static void on_trap(int sig, siginfo_t *si, void *ctx)
{
    (void)sig; (void)ctx;
    why = si->si_code;
    where = si->si_addr;
    /* Simply returning from a trap handler is undefined behaviour (C23 7.14.1.1 p3).
       It would re-run the faulting instruction --- so we jump out instead. */
    siglongjmp(escape, 1);
}

int main(void)
{
    struct sigaction sa = {0};
    sa.sa_sigaction = on_trap;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);

    volatile int zero = 0, x = 1;
    if (sigsetjmp(escape, 1) == 0) {
        x = x / zero;                       /* the CPU raises an exception */
        puts("this line is never reached");
    }
    printf("integer divide: si_code=%d (FPE_INTDIV=%d), faulting instruction=%s\n",
           (int)why, FPE_INTDIV, where ? "reported" : "not reported");

    int *nowhere = NULL;
    if (sigsetjmp(escape, 1) == 0) {
        *nowhere = 1;                       /* same road, a different exception */
        puts("nor is this one");
    }
    printf("null store   : si_code=%d (SEGV_MAPERR=%d), address=%s\n",
           (int)why, SEGV_MAPERR, where == NULL ? "0x0" : "non-zero");

    puts("");
    puts("the path was: CPU exception -> kernel trap handler -> signal -> this C function.");
    puts("three of the six standard signals are hardware exceptions:");
    puts("  SIGFPE (floating-point exception), SIGILL (illegal instruction),");
    puts("  SIGSEGV (segmentation violation).  SIGINT is literally named 'interrupt'.");
    return 0;
}
