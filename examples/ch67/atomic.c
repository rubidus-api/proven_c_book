/* 경쟁 상태와 원자적 연산 — 같은 프로그램을 두 번 돌린다. */
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

#define THREADS 4
#define BUMPS   200000

static long        plain;   /* 그냥 long — 보호 없음 */
static atomic_long safe;    /* 원자적 long        */

static int worker(void *unused)
{
    (void)unused;
    for (int i = 0; i < BUMPS; i++) {
        plain = plain + 1;                    /* 읽고-더하고-쓰기: 쪼개진다 */
        atomic_fetch_add(&safe, 1);           /* 한 덩어리로 일어난다      */
    }
    return 0;
}

int main(void)
{
    thrd_t t[THREADS];

    for (int i = 0; i < THREADS; i++)
        thrd_create(&t[i], worker, NULL);
    for (int i = 0; i < THREADS; i++)
        thrd_join(t[i], NULL);

    long expected = (long)THREADS * BUMPS;
    printf("기대한 값 : %ld\n", expected);
    printf("보호 없음 : %ld%s\n", plain, plain == expected ? "" : "  <- 잃어버렸다");
    printf("원자적    : %ld\n", (long)safe);

    printf("\natomic_long 은 잠금 없이 되는가? %s\n",
           atomic_is_lock_free(&safe) ? "예" : "아니오");
    return 0;
}
