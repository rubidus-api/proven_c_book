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
    puts("[익명 멤버]");
    struct tagged t = { .kind = 1, .i = 42 };
    printf("  t.kind = %u, t.i = %d   ← t.u.i 가 아니라 t.i 로 쓴다\n",
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
        printf("  고리에서 되찾은 작업: \"%s\"(우선순위 %d) — 원본과 같은 주소인가: %s\n",
               task->name, task->priority, task == expect[i] ? "예" : "아니오");
    }

    puts("\n  고리는 자료를 모른다. 그래서 같은 목록 코드가 어떤 구조체에도 붙는다");
    puts("  — 제12부의 침습적 목록이 이 한 줄 위에 서 있다.");
    return 0;
}
