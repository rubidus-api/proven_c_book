/* 「끼어들기에는 값이 있다」를 이 기계에서 *실제로 재어* 본다.
   ★ 주의: 이것은 하드웨어 인터럽트가 아니라 운영체제의 신호다. 자릿수의 감각을 얻는
   용도이고, 하드웨어 IRQ 지연과 같은 수가 아니다(그쪽이 대개 더 짧다). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

static volatile sig_atomic_t hits;
static void handler(int sig) { (void)sig; hits++; }

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static volatile int sink;
static void plain(void) { sink++; }
static void (*volatile plain_p)(void) = plain;

int main(void)
{
    struct sigaction sa = { 0 };
    sa.sa_handler = handler;
    sigaction(SIGUSR1, &sa, NULL);

    const long N = 200000;

    /* 몸풀기 --- 첫 몇 번은 캐시가 차가워 느리다 */
    for (long i = 0; i < 1000; i++) raise(SIGUSR1);

    double t0 = now_ns();
    for (long i = 0; i < N; i++) raise(SIGUSR1);
    double t1 = now_ns();
    double per_signal = (t1 - t0) / (double)N;

    t0 = now_ns();
    for (long i = 0; i < N; i++) plain_p();
    t1 = now_ns();
    double per_call = (t1 - t0) / (double)N;

    printf("== measured on this machine (mean of %ld) ==\n", N);
    printf("  one ordinary function call     : %8.1f ns\n", per_call);
    printf("  raising and handling one signal: %8.1f ns\n", per_signal);
    printf("  ratio                          : %8.1f times\n\n", per_signal / per_call);
    printf("  the handler ran %lld times (a check that none were missed)\n\n",
           (long long)hits);

    printf("== what this number tells you ==\n");
    printf("  that one interruption is not free, and that it costs far more than an\n");
    printf("  ordinary function call --- because each interruption must put the state of\n");
    printf("  the work somewhere, go to the handler, and take it back out again.\n\n");

    printf("  from it, how many per second can be borne:\n");
    printf("  %-16s %-16s %s\n", "events/s", "CPU used", "note");
    const long rates[] = { 1000, 10000, 100000, 1000000 };
    for (unsigned i = 0; i < sizeof rates / sizeof *rates; i++) {
        double busy = per_signal * (double)rates[i] / 1e9 * 100.0;
        char pct[16]; snprintf(pct, sizeof pct, "%.2f%%", busy);
        printf("  %-16ld %-16s %s\n", rates[i], pct,
               busy > 100 ? "impossible --- it would do nothing but handle them"
               : busy > 50 ? "dangerous --- no room for other work"
               : busy > 5  ? "workable, but the margin is thin" : "comfortable");
    }
    printf("\n  * again, this is the cost of an operating system signal. A hardware interrupt\n");
    printf("    is usually far cheaper (a few hundred nanoseconds). But the *shape* of the\n");
    printf("    arithmetic is the same --- learning that shape is the point of this demonstration.\n");
    return 0;
}
