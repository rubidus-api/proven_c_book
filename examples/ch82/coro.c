#include <proven.h>
#include <stdio.h>

/* 스택리스 코루틴: 진행 상태를 구조체 한 칸에 담고, 매크로가 switch 로
   그 자리에 되돌아간다. 스레드도, 별도 스택도, 할당도 없다. */
typedef struct {
    proven_coro_t co;      /* 어디까지 갔는지 기억하는 칸 */
    int           sent;
    int           limit;
} producer_t;

/* 0 을 돌려주면 "값 하나를 내놓고 잠시 멈춤", 1 이면 "다 끝남" */
static int produce(producer_t *p, int *out)
{
    PROVEN_CORO_BEGIN(&p->co);
    while (p->sent < p->limit) {
        *out = p->sent * 10;
        p->sent += 1;
        PROVEN_CORO_YIELD(&p->co);   /* 여기서 나갔다가 다음 호출에 되돌아온다 */
    }
    PROVEN_CORO_END(&p->co);
}

int main(void)
{
    producer_t p = { .sent = 0, .limit = 4 };
    PROVEN_CORO_INIT(&p.co);

    int value = -1;
    while (!PROVEN_CORO_IS_DONE(&p.co)) {
        if (produce(&p, &value) == 0)
            printf("produced %d\n", value);
    }
    printf("finished after %d values; coroutine state is %zu bytes\n",
           p.sent, sizeof(proven_coro_t));
    return 0;
}
