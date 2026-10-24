/* A handler has one job — raise a flag and return at once. */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>          /* write — one of the few things a handler may call */

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t reload_requested;
static volatile sig_atomic_t last_signal;

static void on_signal(int sig)
{
    last_signal = sig;
    if (sig == SIGTERM || sig == SIGINT) stop_requested = 1;
    if (sig == SIGUSR1)                  reload_requested = 1;

    /* That is as far as the standard permits. The write below is allowed only
       because POSIX separately guarantees it async-signal-safe (see the text). */
    static const char note[] = "  [handler] flag set\n";
    ssize_t n = write(STDOUT_FILENO, note, sizeof note - 1);
    (void)n;
}

/* The skeleton of a server — work, look at the flags, decide */
static void serve(void)
{
    int served = 0;
    for (;;) {
        if (reload_requested) {
            reload_requested = 0;
            printf("  reloading configuration (SIGUSR1)\n");
            fflush(stdout);
        }
        if (stop_requested) {
            printf("  cleaning up and going down (after serving %d)\n", served);
            return;
        }
        served++;
        if (served == 2) (void)raise(SIGUSR1);   /* simulate a reload request */
        if (served == 4) (void)raise(SIGTERM);   /* simulate a shutdown request */
    }
}

int main(void)
{
    /* This implementation resets to SIG_DFL after handling, so we re-install.
       (Which is why POSIX code uses sigaction — the next example.) */
    if (signal(SIGUSR1, on_signal) == SIG_ERR) return 1;
    if (signal(SIGTERM, on_signal) == SIG_ERR) return 1;

    puts("server loop starting");
    fflush(stdout);   /* the handler's write bypasses the buffer — keep the order */
    serve();
    printf("last signal received = %d\n", (int)last_signal);

    /* What the handler left behind is one flag — the work happens out here.
       Printing, closing files and freeing all belong in this place. */
    puts("cleanup done outside the loop");
    return 0;
}
