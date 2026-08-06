#include <stdio.h>
#include <stdint.h>

/* 실무에서 흔한 모양: 장치 레지스터 한 워드를 필드로 나눠 보고,
   같은 기억을 통째로 32비트 정수로도 본다. */
union control_reg {
    uint32_t raw;                 /* 통째로 읽고 쓰는 눈 */
    struct {
        uint32_t enable   : 1;    /* 비트 1개 */
        uint32_t mode     : 3;    /* 비트 3개 */
        uint32_t priority : 4;
        uint32_t reserved : 8;
        uint32_t counter  : 16;
    } f;                          /* 필드로 보는 눈 */
};

/* 구조체 안에 공용체를 넣는 실무 무늬: 태그 + 내용 */
enum msg_kind { MSG_INT, MSG_TEXT, MSG_POINT };

struct message {
    enum msg_kind kind;           /* 어느 눈으로 볼지 알려 주는 태그 */
    unsigned      flags : 4;      /* 작은 상태 몇 개 */
    unsigned      urgent : 1;
    union {                       /* 이름 없는 공용체 (C11) */
        int  number;
        char text[16];
        struct { int x, y; } point;
    };
};

static void show(const struct message *m)
{
    printf("kind=%d flags=%u urgent=%u -> ", (int)m->kind, m->flags, m->urgent);
    switch (m->kind) {
        case MSG_INT:   printf("number %d\n", m->number); break;
        case MSG_TEXT:  printf("text \"%s\"\n", m->text); break;
        case MSG_POINT: printf("point (%d, %d)\n", m->point.x, m->point.y); break;
    }
}

int main(void)
{
    union control_reg r = { .raw = 0 };
    r.f.enable = 1;
    r.f.mode = 5;
    r.f.priority = 9;
    r.f.counter = 1000;

    printf("raw   = 0x%08x\n", r.raw);
    printf("fields: enable=%u mode=%u priority=%u counter=%u\n",
           r.f.enable, r.f.mode, r.f.priority, r.f.counter);

    /* 통째로 쓰고 필드로 읽기 — 같은 기억을 두 눈으로 본다 */
    r.raw = 0x000A0013u;
    printf("after raw write: enable=%u mode=%u priority=%u counter=%u\n",
           r.f.enable, r.f.mode, r.f.priority, r.f.counter);

    printf("sizeof(union control_reg) = %zu\n", sizeof r);

    struct message a = { .kind = MSG_INT, .flags = 3, .urgent = 1, .number = 42 };
    struct message b = { .kind = MSG_TEXT, .flags = 0, .urgent = 0 };
    for (int i = 0; i < 5; i++) b.text[i] = "hello"[i];
    b.text[5] = '\0';
    struct message c = { .kind = MSG_POINT, .flags = 8, .urgent = 0,
                         .point = { .x = 3, .y = -7 } };
    show(&a); show(&b); show(&c);
    printf("sizeof(struct message) = %zu\n", sizeof(struct message));
    return 0;
}
