#include <proven.h>
#include <stdio.h>

/* A stackless coroutine: how far it got is held in one slot of a struct, and
   the macros return to that spot with a switch. No thread, no separate stack,
   no allocation. */
typedef struct {
    proven_coro_t co;      /* the slot that remembers how far it got */
    int           sent;
    int           limit;
} producer_t;

/* returning 0 means "one value handed out, paused"; 1 means "all done" */
static int produce(producer_t *p, int *out)
{
    PROVEN_CORO_BEGIN(&p->co);
    while (p->sent < p->limit) {
        *out = p->sent * 10;
        p->sent += 1;
        PROVEN_CORO_YIELD(&p->co);   /* it leaves here and comes back to this spot on the next call */
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
