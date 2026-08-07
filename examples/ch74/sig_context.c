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
    printf("errno 를 챙기지 않는 처리기: 신호 전 %d(%s) → 신호 뒤 %d(%s)\n",
           before, strerror(before), errno, strerror(errno));

    /* ② 저장·복원 */
    signal(SIGUSR1, careful);
    (void)fails();
    before = errno;
    raise(SIGUSR1);
    printf("errno 를 챙기는 처리기:     신호 전 %d(%s) → 신호 뒤 %d(%s)\n",
           before, strerror(before), errno, strerror(errno));

    /* ③ 계산 도중에 끼어들어도 결과가 같다.
       이 구현은 처리 뒤 SIG_DFL 로 되돌아가므로(74장) 다시 걸어 준다. */
    signal(SIGUSR1, careful);
    hits = 0;
    long quiet = compute(0);            /* 신호 없이 */
    long hit   = compute(500);          /* 한복판에서 한 번 맞고 */
    printf("\n조용히 계산한 값 = %ld\n", quiet);
    printf("500회째에 신호를 맞고 계산한 값 = %ld  (처리기 호출 %d회)\n",
           hit, (int)hits);
    puts(quiet == hit ? "같다 — 커널이 레지스터 전부를 저장했다가 되돌려 주었다"
                      : "다르다 — 이럴 리가 없다");

    puts("\nsetjmp/longjmp 와의 차이가 여기 있다(75장):");
    puts("  신호: 커널이 *전체* 레지스터 집합을 저장했다가 복원한다 →");
    puts("        수식 한복판으로 되돌아와도 계산이 이어진다.");
    puts("  longjmp: jmp_buf 에는 *피호출자 보존* 레지스터만 들어간다 →");
    puts("        나머지 레지스터에 있던 지역 변수는 옛 값으로 되돌아간다.");
    return 0;
}
