/* 유연 배열 멤버 — 머리와 데이터를 한 덩어리로 잡는 C99 의 장치. */
#include <stdckdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 마지막 멤버의 크기를 비워 두면 '유연 배열 멤버'다.
   sizeof 에는 이 멤버가 들어가지 않는다 — 크기는 잡을 때 정한다. */
struct msg {
    unsigned kind;
    size_t   len;
    char     data[];      /* ← 유연 배열 멤버 */
};

/* 잡을 크기 = 머리 + 데이터. 넘침 검사를 빠뜨리면 작은 그릇에 큰 배열이 된다. */
static struct msg *msg_new(unsigned kind, const char *text)
{
    size_t len = strlen(text);
    size_t need;
    /* offsetof(struct msg, data) 가 sizeof(struct msg) 보다 정확하다 —
       꼬리 패딩을 두 번 세지 않는다. */
    if (ckd_add(&need, offsetof(struct msg, data), len)) return nullptr;

    struct msg *m = malloc(need);
    if (!m) return nullptr;
    m->kind = kind;
    m->len  = len;
    memcpy(m->data, text, len);
    return m;
}

int main(void)
{
    printf("sizeof(struct msg)          = %zu  <- data is not counted\n",
           sizeof(struct msg));
    printf("offsetof(struct msg, data)  = %zu\n", offsetof(struct msg, data));
    printf("alignof(struct msg)         = %zu\n", alignof(struct msg));

    const char *text = "hello, world!";
    struct msg *m = msg_new(7, text);
    if (!m) { perror("malloc"); return 1; }

    printf("\nsize reserved = offsetof(data) + %zu = %zu bytes\n",
           m->len, offsetof(struct msg, data) + m->len);
    printf("kind = %u, len = %zu, data = \"%.*s\"\n",
           m->kind, m->len, (int)m->len, m->data);

    /* 머리와 데이터가 한 덩어리라 free 도 한 번이다 */
    free(m);

    puts("\ndifference from the old practice:");
    puts("  char data[1];  <- the 'struct hack'. The size arithmetic is off by one,");
    puts("                   and it read past the array, outside the contract.");
    puts("  char data[];   <- what C99 made official. Inside the contract.");
    return 0;
}
