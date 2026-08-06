/* 처리기가 해야 할 일은 하나뿐 — 깃발을 세우고 즉시 돌아간다. */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>          /* write — 처리기 안에서 쓸 수 있는 몇 안 되는 것 */

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t reload_requested;
static volatile sig_atomic_t last_signal;

static void on_signal(int sig)
{
    last_signal = sig;
    if (sig == SIGTERM || sig == SIGINT) stop_requested = 1;
    if (sig == SIGUSR1)                  reload_requested = 1;

    /* 표준이 허락하는 것은 여기까지다. 아래 write 는 POSIX 가 따로 허락한
       비동기-신호-안전 함수라서 예외적으로 쓸 수 있다(본문 참조). */
    static const char note[] = "  [handler] flag set\n";
    ssize_t n = write(STDOUT_FILENO, note, sizeof note - 1);
    (void)n;
}

/* 서버의 뼈대 — 일하다가, 깃발을 보고, 결정한다 */
static void serve(void)
{
    int served = 0;
    for (;;) {
        if (reload_requested) {
            reload_requested = 0;
            printf("  설정을 다시 읽는다(SIGUSR1)\n");
            fflush(stdout);
        }
        if (stop_requested) {
            printf("  정리하고 내려간다(%d번 처리한 뒤)\n", served);
            return;
        }
        served++;
        if (served == 2) (void)raise(SIGUSR1);   /* 재적재 요청을 흉내 낸다 */
        if (served == 4) (void)raise(SIGTERM);   /* 종료 요청을 흉내 낸다 */
    }
}

int main(void)
{
    /* 이 구현은 처리 뒤 SIG_DFL 로 되돌리므로, 매번 다시 건다.
       (그래서 POSIX 코드는 sigaction 을 쓴다 — 다음 예제) */
    if (signal(SIGUSR1, on_signal) == SIG_ERR) return 1;
    if (signal(SIGTERM, on_signal) == SIG_ERR) return 1;

    puts("서버 루프 시작");
    fflush(stdout);   /* 처리기의 write 는 버퍼를 거치지 않는다 — 순서를 맞춘다 */
    serve();
    printf("마지막으로 받은 신호 번호 = %d\n", (int)last_signal);

    /* 처리기가 남기고 간 것은 '깃발' 하나다 — 그 값으로 밖에서 일한다.
       인쇄도, 파일 닫기도, free 도 전부 여기서 한다. */
    puts("루프 밖에서 정리 완료");
    return 0;
}
