/* Anonymous members and container_of — from a member's address back to the struct. */
#include <stddef.h>
#include <stdio.h>

/* The link of an intrusive list. Meaningless alone — it is embedded in others. */
struct link { struct link *next; };

/* C11's anonymous members: reach an unnamed struct's or union's members directly */
struct tagged {
    unsigned kind;
    union {                       /* <- no name */
        int    i;
        double d;
    };
};

struct task {
    const char  *name;
    int          priority;
    struct link  node;            /* the link it hangs from */
};

/* The member's address minus that member's offset is the struct's address.
   The Linux kernel's container_of is this one line. */
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

    /* the list knows only links; recover the task from the link */
    printf("  offsetof(struct task, node) = %zu\n", offsetof(struct task, node));
    struct task *expect[] = { &a, &b };
    size_t i = 0;
    for (struct link *p = &a.node; p; p = p->next, i++) {
        struct task *task = CONTAINER_OF(p, struct task, node);
        printf("  recovered from the link: \"%s\" (priority %d) — same address? %s\n",
               task->name, task->priority, task == expect[i] ? "yes" : "no");
    }

    puts("\n  The link knows nothing of the data, so one list works for any struct");
    puts("  — Part XII's intrusive list stands on this one line.");
    return 0;
}
