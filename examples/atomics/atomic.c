/* 원자적 갱신 — 네 갈래가 한 카운터를 쪼개지지 않는 연산으로만 늘린다.
   보호 없는 카운터의 경쟁은 94장의 첫 시연이 보였다. 같은 실행에 섞지 않는다:
   경쟁이 하나라도 있으면 그 실행 전체가 정의되지 않으므로, 옆의 원자 카운터 값도
   아무것도 증명하지 못한다. */
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#define THREADS 4
#define BUMPS   200000

static atomic_long count;   /* 원자적 long */

static int worker(void *unused)
{
    (void)unused;
    for (int i = 0; i < BUMPS; i++)
        atomic_fetch_add(&count, 1);          /* 읽고-더하고-쓰기가 한 덩어리로 일어난다 */
    return 0;
}

int main(void)
{
    thrd_t t[THREADS];
    int made = 0;

    for (; made < THREADS; made++)
        if (thrd_create(&t[made], worker, NULL) != thrd_success)
            break;
    /* 만든 갈래는 실패했든 아니든 모두 기다린다 */
    bool joined = true;
    for (int i = 0; i < made; i++)
        if (thrd_join(t[i], NULL) != thrd_success)
            joined = false;
    if (made < THREADS || !joined) {
        fprintf(stderr, "created %d of %d threads, joined all: %s\n",
                made, THREADS, joined ? "yes" : "no");
        return EXIT_FAILURE;
    }

    /* 여기까지 왔으면 네 갈래 모두 만들어졌고 모두 끝났다 --- 이때에만 80만이 보장된다 */
    printf("expected: %ld\n", (long)THREADS * BUMPS);
    printf("atomic  : %ld\n", (long)atomic_load(&count));

    printf("\nis atomic_long lock-free? %s\n",
           atomic_is_lock_free(&count) ? "yes" : "no");
    return 0;
}
