#include <stdio.h>
#include <stdint.h>

/* A shape common in practice: one device register word divided into fields,
   with the same memory also seen whole as a 32-bit integer. */
union control_reg {
    uint32_t raw;                 /* the eye that reads and writes it whole */
    struct {
        uint32_t enable   : 1;    /* one bit */
        uint32_t mode     : 3;    /* three bits */
        uint32_t priority : 4;
        uint32_t reserved : 8;
        uint32_t counter  : 16;
    } f;                          /* the eye that sees fields */
};

/* the practical pattern of a union inside a struct: a tag plus the content */
enum msg_kind { MSG_INT, MSG_TEXT, MSG_POINT };

struct message {
    enum msg_kind kind;           /* the tag telling which eye to look through */
    unsigned      flags : 4;      /* a few small states */
    unsigned      urgent : 1;
    union {                       /* an anonymous union (C11) */
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

    /* write it whole and read it as fields — the same memory through two eyes */
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
