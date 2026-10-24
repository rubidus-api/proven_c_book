/* 신호는 무엇을 저장하고 무엇을 되돌리는가 — 레지스터 문맥과 errno. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t hits;

/* ── ① errno 를 챙기지 않는 처리기 ─────────────────────────────
   write 는 처리기에서 쓸 수 있는 몇 안 되는 함수지만, 실패하면 errno 를
   바꾼다. 주 흐름이 그 값을 보기 직전이었다면 진단이 통째로 뒤집힌다. */
static void careless(int sig)
{
    (void)sig;
    hits++;
    ssize_t n = write(-1, "", 1);   /* 반드시 실패한다 — errno = EBADF */
    (void)n;
}

/* ── ② errno 를 저장하고 복원하는 처리기 ─────────────────────── */
static void careful(int sig)
{
    int saved = errno;              /* 들어오자마자 챙긴다 */
    (void)sig;
    hits++;
    ssize_t n = write(-1, "", 1);
    (void)n;
    errno = saved;                  /* 나가기 직전에 되돌린다 */
}

/* ── 레지스터에 얹혀 도는 계산. 중간에 신호를 맞아도 결과가 같은가? ── */
static long compute(int interrupt_at)
{
    long acc = 0;
    for (int i = 1; i <= 1000; i++) {
        acc += (long)i * i % 7;
        if (i == interrupt_at) raise(SIGUSR1);
    }
    return acc;
}

static int fails(void)              /* errno 를 ENOENT 로 만들어 두는 실패 */
{
    errno = 0;
    return access("/no/such/file/here", F_OK);
}

int main(void)
{
    /* ① errno 오염 */
    signal(SIGUSR1, careless);
    (void)fails();
    int before = errno;
    raise(SIGUSR1);
    printf("a handler that does not save errno: before %d(%s) -> after %d(%s)\n",
           before, strerror(before), errno, strerror(errno));

    /* ② 저장·복원 */
    signal(SIGUSR1, careful);
    (void)fails();
    before = errno;
    raise(SIGUSR1);
    printf("a handler that does save errno:     before %d(%s) -> after %d(%s)\n",
           before, strerror(before), errno, strerror(errno));

    /* ③ 계산 도중에 끼어들어도 결과가 같다.
       이 구현은 처리 뒤 SIG_DFL 로 되돌아가므로(76장) 다시 걸어 준다. */
    signal(SIGUSR1, careful);
    hits = 0;
    long quiet = compute(0);            /* 신호 없이 */
    long hit   = compute(500);          /* 한복판에서 한 번 맞고 */
    printf("\nvalue computed undisturbed = %ld\n", quiet);
    printf("value computed while hit by a signal at iteration 500 = %ld  (handler called %d times)\n",
           hit, (int)hits);
    puts(quiet == hit ? "the same - the kernel saved every register and gave them back"
                      : "different - this should not happen");

    puts("\nthis is where it differs from setjmp/longjmp (chapter 77):");
    puts("  a signal: the kernel saves and restores the *whole* register set ->");
    puts("        you can return into the middle of an expression and the arithmetic continues.");
    puts("  longjmp: a jmp_buf holds only the *callee-saved* registers ->");
    puts("        locals that lived in the other registers snap back to their old values.");
    return 0;
}
