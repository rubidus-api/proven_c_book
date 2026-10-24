/* The standard part of <signal.h> — three dispositions and raise. */
#include <signal.h>
#include <stdio.h>
#include <string.h>

/* This is all the standard permits in a handler's body: assigning to a
   volatile sig_atomic_t. Anything else is outside the contract. */
static volatile sig_atomic_t got_int;
static volatile sig_atomic_t got_term;

static void on_signal(int sig)
{
    if (sig == SIGINT)  got_int  = 1;
    if (sig == SIGTERM) got_term = 1;
    /* no printf here — the text explains why */
}

static const char *disposition(void (*h)(int))
{
    if (h == SIG_DFL) return "SIG_DFL (default handling)";
    if (h == SIG_IGN) return "SIG_IGN (ignore)";
    if (h == SIG_ERR) return "SIG_ERR (setting failed)";
    return "my handler";
}

int main(void)
{
    /* -- (1) install — what comes back is the *previous* disposition ---- */
    void (*prev)(int) = signal(SIGINT, on_signal);
    printf("signal(SIGINT, on_signal) returned %s\n", disposition(prev));

    /* -- (2) raise — send a signal to yourself -------------------------- */
    printf("before raise(SIGINT): got_int = %d\n", (int)got_int);
    int r = raise(SIGINT);
    printf("raise returned %d (0 means success), got_int = %d\n", r, (int)got_int);

    /* -- (3) ignore — with SIG_IGN the signal simply vanishes ----------- */
    (void)signal(SIGTERM, SIG_IGN);
    (void)raise(SIGTERM);
    printf("after SIG_IGN, raise(SIGTERM): got_term = %d (still 0)\n",
           (int)got_term);

    /* -- (4) does the handler survive? — implementation-defined (§7.14.1.1) */
    void (*was)(int) = signal(SIGINT, prev);
    printf("\nafter handling one signal, what was installed: %s\n", disposition(was));
    printf("  -> this implementation resets to SIG_DFL (the old System V way).\n");
    printf("  -> the standard allows either, so portable code re-installs in\n");
    printf("     the handler, or uses POSIX sigaction.\n");

    /* -- (5) the standard defines only six signals ---------------------- */
    struct { int num; const char *name; const char *meaning; } table[] = {
        { SIGABRT, "SIGABRT", "abnormal termination (abort)" },
        { SIGFPE,  "SIGFPE",  "arithmetic error (divide by zero, ...)" },
        { SIGILL,  "SIGILL",  "invalid instruction" },
        { SIGINT,  "SIGINT",  "interactive attention (Ctrl+C)" },
        { SIGSEGV, "SIGSEGV", "invalid memory access" },
        { SIGTERM, "SIGTERM", "termination request" },
    };
    printf("\nthe %zu signals the standard defines:\n", sizeof table / sizeof *table);
    for (size_t i = 0; i < sizeof table / sizeof *table; i++)
        printf("  %-8s = %2d   %s\n", table[i].name, table[i].num, table[i].meaning);

    printf("\nsizeof(sig_atomic_t) = %zu bytes\n", sizeof(sig_atomic_t));
    return 0;
}
