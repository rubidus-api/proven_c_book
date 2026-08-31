/* 인터럽트는 세지 않는다 --- 대기 비트가 하나뿐이다. */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t runs = 0;
static void tick(int s) { (void)s; runs++; }

int main(void)
{
    signal(SIGUSR1, tick);

    sigset_t all, saved;
    sigfillset(&all);
    sigprocmask(SIG_BLOCK, &all, &saved);   /* 막아 둔다 --- 하드웨어의 '금지'에 해당 */

    for (int i = 0; i < 10; i++) raise(SIGUSR1);   /* 열 번 요청한다 */
    printf("raised 10 times while blocked\n");

    sigprocmask(SIG_SETMASK, &saved, NULL);  /* 푼다 --- 밀린 것이 쏟아질까? */
    printf("handler ran %d time(s) after unblocking\n", (int)runs);
    puts("");
    puts("a pending interrupt is a bit, not a counter: ten requests, one delivery.");
    puts("hardware behaves the same way, which is why a handler must ask the device");
    puts("what happened rather than assume it ran once per event.");
    return 0;
}
