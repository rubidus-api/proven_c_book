/* 하드웨어 예외가 C 함수까지 올라오는 길 --- OS 위에서 그 길을 눈으로 본다. */
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

static sigjmp_buf escape;
static volatile sig_atomic_t why = 0;
static void *where = NULL;

/* 처리기는 하드웨어가 준 것을 받는다 --- 원인 번호와 터진 명령의 주소 */
static void on_trap(int sig, siginfo_t *si, void *ctx)
{
    (void)sig; (void)ctx;
    why = si->si_code;
    where = si->si_addr;
    /* 트랩 처리기에서 그냥 돌아오는 것은 미정의 동작이다(C23 7.14.1.1 p3).
       터진 명령을 다시 실행하려 들기 때문이다 --- 그래서 뛰어서 빠져나온다. */
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
        x = x / zero;                       /* CPU 가 예외를 일으킨다 */
        puts("this line is never reached");
    }
    printf("integer divide: si_code=%d (FPE_INTDIV=%d), faulting instruction=%s\n",
           (int)why, FPE_INTDIV, where ? "reported" : "not reported");

    int *nowhere = NULL;
    if (sigsetjmp(escape, 1) == 0) {
        *nowhere = 1;                       /* 같은 길, 다른 예외 */
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
