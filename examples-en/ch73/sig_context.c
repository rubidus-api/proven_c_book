/* what a signal saves and restores — the register context, and errno. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t hits;

/* ── (1) a handler that does not look after errno ──────────────────
   write is one of the few functions a handler may call, but on failure it
   changes errno. If the main flow was about to read that value, the whole
   diagnosis is turned upside down. */
static void careless(int sig)
{
    (void)sig;
    hits++;
    ssize_t n = write(-1, "", 1);   /* always fails — errno = EBADF */
    (void)n;
}

/* ── (2) a handler that saves and restores errno ─────────────────── */
static void careful(int sig)
{
    int saved = errno;              /* saved on the way in */
    (void)sig;
    hits++;
    ssize_t n = write(-1, "", 1);
    (void)n;
    errno = saved;                  /* restored on the way out */
}

/* ── a computation living in registers. same result if a signal cuts in? ── */
static long compute(int interrupt_at)
{
    long acc = 0;
    for (int i = 1; i <= 1000; i++) {
        acc += (long)i * i % 7;
        if (i == interrupt_at) raise(SIGUSR1);
    }
    return acc;
}

static int fails(void)              /* a failure that leaves errno as ENOENT */
{
    errno = 0;
    return access("/no/such/file/here", F_OK);
}

int main(void)
{
    /* (1) errno gets clobbered */
    signal(SIGUSR1, careless);
    (void)fails();
    int before = errno;
    raise(SIGUSR1);
    printf("handler that ignores errno: before %d(%s) -> after %d(%s)\n",
           before, strerror(before), errno, strerror(errno));

    /* (2) saved and restored */
    signal(SIGUSR1, careful);
    (void)fails();
    before = errno;
    raise(SIGUSR1);
    printf("handler that saves errno:   before %d(%s) -> after %d(%s)\n",
           before, strerror(before), errno, strerror(errno));

    /* (3) the result is the same even when a signal cuts in.
       This implementation reverts to SIG_DFL after handling (chapter 73),
       so the handler is installed again. */
    signal(SIGUSR1, careful);
    hits = 0;
    long quiet = compute(0);            /* without a signal */
    long hit   = compute(500);          /* hit once, right in the middle */
    printf("\ncomputed quietly            = %ld\n", quiet);
    printf("computed with a signal at 500 = %ld  (handler ran %d time)\n",
           hit, (int)hits);
    puts(quiet == hit ? "the same - the kernel saved and restored every register"
                      : "different - this should not happen");

    puts("\nHere is the difference from setjmp/longjmp (chapter 74):");
    puts("  signal:  the kernel saves and restores the *whole* register set ->");
    puts("           returning into the middle of an expression just works.");
    puts("  longjmp: jmp_buf holds only the *callee-saved* registers ->");
    puts("           locals living in the others revert to their old values.");
    return 0;
}
