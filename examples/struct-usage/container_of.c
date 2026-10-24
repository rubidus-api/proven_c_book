/* 익명 멤버와 container_of — 멤버의 주소에서 바깥 구조체를 되찾는다. */
#include <stddef.h>
#include <stdio.h>

/* 침습적 목록의 고리. 이 자체로는 아무 뜻이 없다 — 남의 구조체에 박힌다. */
struct link { struct link *next; };

/* C11 의 익명 멤버: 이름 없는 구조체·공용체의 멤버를 바깥에서 바로 쓴다 */
struct tagged {
    unsigned kind;
    union {                       /* ← 이름이 없다 */
        int    i;
        double d;
    };
};

struct task {
    const char  *name;
    int          priority;
    struct link  node;            /* 목록에 매달리는 고리 */
};

/* 멤버의 주소 − 그 멤버의 오프셋 = 바깥 구조체의 주소.
   리눅스 커널의 container_of 가 이 한 줄이다. */
#define CONTAINER_OF(ptr, type, member) \
    ((type *)(void *)((char *)(ptr) - offsetof(type, member)))

int main(void)
{
    puts("[anonymous members]");
    struct tagged t = { .kind = 1, .i = 42 };
    printf("  t.kind = %u, t.i = %d   <- written t.i, not t.u.i\n",
           t.kind, t.i);
    printf("  sizeof(struct tagged) = %zu\n", sizeof t);

    puts("\n[container_of]");
    struct task a = { .name = "compile", .priority = 3 };
    struct task b = { .name = "link",    .priority = 1 };
    a.node.next = &b.node;
    b.node.next = nullptr;

    /* 목록은 고리만 안다. 고리에서 작업을 되찾는다. */
    printf("  offsetof(struct task, node) = %zu\n", offsetof(struct task, node));
    struct task *expect[] = { &a, &b };
    size_t i = 0;
    for (struct link *p = &a.node; p; p = p->next, i++) {
        struct task *task = CONTAINER_OF(p, struct task, node);
        printf("  task recovered from the link: \"%s\" (priority %d) - same address as the original: %s\n",
               task->name, task->priority, task == expect[i] ? "yes" : "no");
    }

    puts("\n  the link knows nothing about the data, so the same list code fits any struct");
    puts("  - the intrusive list of part 12 stands on this one line.");
    return 0;
}
